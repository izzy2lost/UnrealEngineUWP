// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Algo/Transform.h"
#include "Async/Async.h"
#include "Clients/LiveLinkHubClientsModel.h"
#include "Clients/LiveLinkHubUEClientInfo.h"
#include "Containers/ObservableArray.h"
#include "CoreMinimal.h"
#include "Delegates/DelegateCombinations.h"
#include "Editor.h"
#include "Engine/TimerHandle.h"
#include "GameThreadMessageHandler.h"
#include "HAL/CriticalSection.h"
#include "IMessageContext.h"
#include "LiveLinkHubLog.h"
#include "LiveLinkHubMessages.h"
#include "LiveLinkProviderImpl.h"
#include "LiveLinkSettings.h"
#include "MessageEndpointBuilder.h"
#include "Misc/ScopeLock.h"
#include "Subjects/LiveLinkHubSubjectSessionConfig.h"
#include "Session/LiveLinkHubSession.h"
#include "TimerManager.h"

#define LOCTEXT_NAMESPACE "LiveLinkHub.LiveLinkHubProvider"

/** 
 * LiveLink Provider that allows getting more information about a UE client by communicating with a LiveLinkHub MessageBus Source.
 */
class FLiveLinkHubProvider : public FLiveLinkProvider, public ILiveLinkHubClientsModel, public TSharedFromThis<FLiveLinkHubProvider>
{
public:
	using FLiveLinkProvider::SendClearSubjectToConnections;
	using FLiveLinkProvider::GetLastSubjectStaticDataStruct;

	/**
	 * Create a message bus handler that will dispatch messages on the game thread. 
	 * This is useful to receive some messages on AnyThread and delegate others on the game thread (ie. for methods that will trigger UI updates which need to happen on game thread. )
	 */
	template <typename MessageType>
	TSharedRef<TGameThreadMessageHandler<MessageType, FLiveLinkHubProvider>> MakeHandler(typename TGameThreadMessageHandler<MessageType, FLiveLinkHubProvider>::FuncType Func)
	{
		return MakeShared<TGameThreadMessageHandler<MessageType, FLiveLinkHubProvider>>(this, Func);
	}

	FLiveLinkHubProvider(const TSharedRef<ILiveLinkHubSessionManager>& InSessionManager)
		: FLiveLinkProvider(TEXT("LiveLink Hub"), false)
		, SessionManager(InSessionManager)
	{
		Annotations.Add(FLiveLinkHubMessageAnnotation::ProviderTypeAnnotation, UE::LiveLinkHub::Private::LiveLinkHubProviderType.ToString());

		FMessageEndpointBuilder EndpointBuilder = FMessageEndpoint::Builder(*GetProviderName());
		EndpointBuilder.WithHandler(MakeHandler<FLiveLinkClientInfoMessage>(&FLiveLinkHubProvider::HandleClientInfoMessage));
		EndpointBuilder.WithHandler(MakeHandler<FLiveLinkHubConnectMessage>(&FLiveLinkHubProvider::HandleHubConnectMessage));

		CreateMessageEndpoint(EndpointBuilder);

		const double ValidateConnectionsRate = GetDefault<ULiveLinkSettings>()->MessageBusPingRequestFrequency;
		GEditor->GetTimerManager()->SetTimer(ValidateConnectionsTimer, FTimerDelegate::CreateRaw(this, &FLiveLinkHubProvider::ValidateConnections), ValidateConnectionsRate, true);
	}

	virtual ~FLiveLinkHubProvider() override
	{
		if (GEditor)
		{
			GEditor->GetTimerManager()->ClearTimer(ValidateConnectionsTimer);
		}
	}

	virtual bool ShouldTransmitToSubject_AnyThread(FName SubjectName, FMessageAddress Address) const override
	{
		FReadScopeLock Locker(ClientsMapLock);
		if (const FLiveLinkHubUEClientInfo* ClientInfoPtr = ClientsMap.Find(Address))
		{
			if (!ClientInfoPtr->bEnabled)
			{
				return false;
			}

			return !ClientInfoPtr->DisabledSubjects.Contains(SubjectName);
		}
		else
		{
			UE_LOG(LogLiveLinkHub, Warning, TEXT("Attempted to transmit data to an invalid client."));
		}

		return true;
	}

	/** Manually add a client to the client map. */
	void AddClient(const FLiveLinkHubUEClientInfo& InClientInfo, const FMessageAddress& InMessageAddress)
	{
		ClientsMap.Add(InMessageAddress, InClientInfo);
		OnClientEventDelegate.Broadcast(InMessageAddress, EClientEventType::Connected);
	}

	/** Manually remove a client from the client map. */
	void RemoveClient(const FMessageAddress& InMessageAddress)
	{
		ClientsMap.Remove(InMessageAddress);
		OnClientEventDelegate.Broadcast(InMessageAddress, EClientEventType::Disconnected);
	}

	/** Retrieve the existing client map. */
	const TMap<FMessageAddress, FLiveLinkHubUEClientInfo>& GetClientsMap() const { return ClientsMap; }

private:
	/** Handle a connection message resulting from a livelink hub message bus source connecting to this provider. */
	void HandleHubConnectMessage(const FLiveLinkHubConnectMessage& Message, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context)
	{
		FLiveLinkConnectMessage ConnectMessage;
		ConnectMessage.LiveLinkVersion = Message.ClientInfo.LiveLinkVersion;
		FLiveLinkProvider::HandleConnectMessage(ConnectMessage, Context);

		FMessageAddress ConnectionAddress = Context->GetSender();


		TOptional<FMessageAddress> RemovedAddress;
		{
			FWriteScopeLock Locker(ClientsMapLock);
			// Remove old entries if one is found
            for (auto It = ClientsMap.CreateIterator(); It; ++It)
            {
            	FLiveLinkHubUEClientInfo& IteratedClient = It->Value;
            	if (IteratedClient.Hostname == Message.ClientInfo.Hostname && IteratedClient.LongName == Message.ClientInfo.LongName
            		&& IteratedClient.ProjectName == Message.ClientInfo.ProjectName && IteratedClient.CurrentLevel == Message.ClientInfo.CurrentLevel)
            	{
            		RemovedAddress = It->Key;
            		It.RemoveCurrent();
            		break;
            	}
            }
		}

		ClientsMap.Add(ConnectionAddress, FLiveLinkHubUEClientInfo(Message.ClientInfo, ConnectionAddress));

		if (RemovedAddress)
		{
			OnClientEventDelegate.Broadcast(*RemovedAddress, EClientEventType::Disconnected);
		}
		OnClientEventDelegate.Broadcast(ConnectionAddress, EClientEventType::Connected);
	}
	
	/** Handle a client info message being received. Happens when new information about a client is received (ie. Client has changed map) */
	void HandleClientInfoMessage(const FLiveLinkClientInfoMessage& Message, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context)
	{
		FWriteScopeLock Locker(ClientsMapLock);
		FMessageAddress Address = Context->GetSender();
		if (FLiveLinkHubUEClientInfo* ClientInfo = ClientsMap.Find(Address))
		{
			*ClientInfo = FLiveLinkHubUEClientInfo(Message, Address);
		}

		OnClientEventDelegate.Broadcast(Address, EClientEventType::Modified);
	}

protected:
	//~ Begin FLiveLinkProvider interface
	virtual void OnConnectionsClosed(const TArray<FMessageAddress>& ClosedAddresses) override
	{
		TArray<FMessageAddress> Notifications;
		{
			FWriteScopeLock Locker(ClientsMapLock);
			// todo: If we want to show disconnected clients, we should update the status in the clients map rather than remove it.
			for (FMessageAddress TrackedAddress : ClosedAddresses)
			{
				if (ClientsMap.Contains(TrackedAddress))
				{
					ClientsMap.Remove(TrackedAddress);
					Notifications.Add(TrackedAddress);
				}
			}
		}

		for (FMessageAddress TrackedAddress : Notifications)
		{
			OnClientEventDelegate.Broadcast(TrackedAddress, EClientEventType::Modified);
		}
	}

	virtual TMap<FName, FString> GetAnnotations() const override
	{
		return Annotations;
	}

	virtual TArray<FMessageAddress> GetClients() const override
	{
		FReadScopeLock Locker(ClientsMapLock);
		TArray<FMessageAddress> ClientAddresses;
		ClientsMap.GenerateKeyArray(ClientAddresses);
		return ClientAddresses;
	}

	virtual TOptional<FLiveLinkHubUEClientInfo> GetClientInfo(FMessageAddress InAddress) const override
	{
		FReadScopeLock Locker(ClientsMapLock);
		TOptional<FLiveLinkHubUEClientInfo> ClientInfo;
		if (const FLiveLinkHubUEClientInfo* ClientInfoPtr = ClientsMap.Find(InAddress))
		{
			ClientInfo = *ClientInfoPtr;
		}

		return ClientInfo;
	}
	virtual FOnClientEvent& OnClientEvent() override
	{
		return OnClientEventDelegate;
	}

	virtual FText GetClientStatus(FMessageAddress Client) const override
	{
		FReadScopeLock Locker(ClientsMapLock);
		if (const FLiveLinkHubUEClientInfo* ClientInfoPtr = ClientsMap.Find(Client))
		{
			return StaticEnum<ELiveLinkClientStatus>()->GetDisplayNameTextByValue(static_cast<int64>(ClientInfoPtr->Status));
		}
		
		return LOCTEXT("InvalidStatus", "Invalid");
	}

	/** Get whether a client should receive livelink data. */
	virtual bool IsClientEnabled(FMessageAddress Client) const override
	{
		FReadScopeLock Locker(ClientsMapLock);
		if (const FLiveLinkHubUEClientInfo* ClientInfoPtr = ClientsMap.Find(Client))
		{
			return ClientInfoPtr->bEnabled;
		}
		return false;
	}

	/** Set whether a client should receive livelink data. */
	virtual void SetClientEnabled(FMessageAddress Client, bool bInEnable) override
	{
		FWriteScopeLock Locker(ClientsMapLock);
		if (FLiveLinkHubUEClientInfo* ClientInfoPtr = ClientsMap.Find(Client))
		{
			ClientInfoPtr->bEnabled = bInEnable;
		}
	}

	/** Get whether a subject is enabled on a given client. */
	virtual bool IsSubjectEnabled(FMessageAddress Client, const FLiveLinkSubjectKey& Subject) const override
	{
		FReadScopeLock Locker(ClientsMapLock);
		if (const FLiveLinkHubUEClientInfo* ClientInfoPtr = ClientsMap.Find(Client))
		{
			return !ClientInfoPtr->DisabledSubjects.Contains(Subject.SubjectName);
		}
		return false;
	}

	/** Set whether a subject should receive livelink data. */
	virtual void SetSubjectEnabled(FMessageAddress Client, const FLiveLinkSubjectKey& Subject, bool bInEnable) override
	{
		FWriteScopeLock Locker(ClientsMapLock);
		if (FLiveLinkHubUEClientInfo* ClientInfoPtr = ClientsMap.Find(Client))
		{
			if (bInEnable)
			{
				ClientInfoPtr->DisabledSubjects.Remove(Subject.SubjectName);
			}
			else
			{
				ClientInfoPtr->DisabledSubjects.Add(Subject.SubjectName);
			}
		}
	}
	//~ End FLiveLinkProvider interface

private:
	/** Handle to the timer responsible for validating the livelinkprovider's connections.*/
	FTimerHandle ValidateConnectionsTimer;
	/** List of information we haved on clients we have discovered. */
	TMap<FMessageAddress, FLiveLinkHubUEClientInfo> ClientsMap;
	/** Delegate called when the provider receives a client change. */
	FOnClientEvent OnClientEventDelegate;
	/** Annotations sent with every message from this provider. In our case it's use to disambiguate a livelink hub provider from other livelink providers.*/
	TMap<FName, FString> Annotations;
	/** LiveLinkHub session manager. */
	TWeakPtr<ILiveLinkHubSessionManager> SessionManager;
	/** Lock used to access the clients map from different threads. */
	mutable FRWLock ClientsMapLock;
};

#undef LOCTEXT_NAMESPACE /*LiveLinkHub.LiveLinkHubProvider*/
