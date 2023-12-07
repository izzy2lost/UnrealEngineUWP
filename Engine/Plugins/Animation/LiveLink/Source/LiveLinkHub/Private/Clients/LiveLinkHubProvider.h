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
		if (const FLiveLinkHubUEClientInfo* ClientInfoPtr = ClientsMap.FindByHash(GetTypeHash(Address), Address))
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
	void AddClient(const FLiveLinkHubUEClientInfo& InClientInfo)
	{
		{
			FWriteScopeLock Locker(ClientsMapLock);
			ClientsMap.Add(InClientInfo.Id, InClientInfo);
		}

		OnClientEventDelegate.Broadcast(InClientInfo.Id, EClientEventType::Connected);
	}

	/** Manually remove a client from the client map. */
	virtual void RemoveClient(FLiveLinkHubClientId InClientId) override
	{
		{
			// Remove the client from the map so that the connection closed callback doesn't set it to disconnected status.
			FWriteScopeLock Locker(ClientsMapLock);
			ClientsMap.Remove(InClientId);
		}

		CloseConnection(InClientId.GetAddress());

		OnClientEventDelegate.Broadcast(InClientId, EClientEventType::Removed);
	}

	/** Retrieve the existing client map. */
	const TMap<FLiveLinkHubClientId, FLiveLinkHubUEClientInfo>& GetClientsMap() const { return ClientsMap; }

private:
	/** Handle a connection message resulting from a livelink hub message bus source connecting to this provider. */
	void HandleHubConnectMessage(const FLiveLinkHubConnectMessage& Message, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context)
	{
		FLiveLinkConnectMessage ConnectMessage;
		ConnectMessage.LiveLinkVersion = Message.ClientInfo.LiveLinkVersion;
		FLiveLinkProvider::HandleConnectMessage(ConnectMessage, Context);

		const FMessageAddress ConnectionAddress = Context->GetSender();

		TOptional<FLiveLinkHubClientId> UpdatedClient;
		{
			FWriteScopeLock Locker(ClientsMapLock);
			// Remove old entries if one is found
            for (auto It = ClientsMap.CreateIterator(); It; ++It)
            {
            	FLiveLinkHubUEClientInfo& IteratedClient = It->Value;
            	if (IteratedClient.Hostname == Message.ClientInfo.Hostname && IteratedClient.LongName == Message.ClientInfo.LongName
            		&& IteratedClient.ProjectName == Message.ClientInfo.ProjectName && IteratedClient.CurrentLevel == Message.ClientInfo.CurrentLevel)
            	{
            		// Only replace disconnected clients to support multiple UE instances on the same host.
					if (IteratedClient.Status == ELiveLinkClientStatus::Disconnected)
					{
						FLiveLinkHubUEClientInfo RemovedInfo = MoveTemp(It->Value);
						RemovedInfo.Id.InvalidateAddress();
						RemovedInfo.Status = ELiveLinkClientStatus::Connected;
						ClientsMap.Remove(It->Key);

						FLiveLinkHubClientId NewId = RemovedInfo.Id;
						ClientsMap.Add(NewId, MoveTemp(RemovedInfo));
						UpdatedClient = NewId;
            			break;
					}
            	}
            }
		}

		if (UpdatedClient)
		{
			OnClientEventDelegate.Broadcast(*UpdatedClient, EClientEventType::Modified);
		}
		else
		{
			FLiveLinkHubUEClientInfo NewClient{Message.ClientInfo, ConnectionAddress};
			const FLiveLinkHubClientId NewClientId = NewClient.Id;
			{
				FWriteScopeLock Locker(ClientsMapLock);
				ClientsMap.Add(NewClient.Id, MoveTemp(NewClient));
			}

			OnClientEventDelegate.Broadcast(NewClientId, EClientEventType::Connected);
		}
	}
	
	/** Handle a client info message being received. Happens when new information about a client is received (ie. Client has changed map) */
	void HandleClientInfoMessage(const FLiveLinkClientInfoMessage& Message, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context)
	{
		FMessageAddress Address = Context->GetSender();
		FLiveLinkHubClientId Id(Address);

		{
			FWriteScopeLock Locker(ClientsMapLock);
			if (FLiveLinkHubUEClientInfo* ClientInfo = ClientsMap.Find(Id))
			{
				*ClientInfo = FLiveLinkHubUEClientInfo(Message, Address);
			}
		}
		OnClientEventDelegate.Broadcast(Id, EClientEventType::Modified);
	}

protected:
	//~ Begin FLiveLinkProvider interface
	virtual void OnConnectionsClosed(const TArray<FMessageAddress>& ClosedAddresses) override
	{
		TArray<FLiveLinkHubClientId> Notifications;
		{
			FWriteScopeLock Locker(ClientsMapLock);

			for (FMessageAddress TrackedAddress : ClosedAddresses)
			{
				const FLiveLinkHubClientId Id{ TrackedAddress };

				if (FLiveLinkHubUEClientInfo* FoundInfo = ClientsMap.Find(Id))
				{
					FLiveLinkHubUEClientInfo RemovedInfo = MoveTemp(*FoundInfo);
					RemovedInfo.Id.InvalidateAddress();
					RemovedInfo.Status = ELiveLinkClientStatus::Disconnected;
					ClientsMap.Remove(Id);

					FLiveLinkHubClientId NewId = RemovedInfo.Id;
					ClientsMap.Add(NewId, MoveTemp(RemovedInfo));
					Notifications.Add(Id);
				}
			}
		}

		for (FLiveLinkHubClientId Id : Notifications)
		{
			OnClientEventDelegate.Broadcast(Id, EClientEventType::Modified);
		}
	}

	virtual TMap<FName, FString> GetAnnotations() const override
	{
		return Annotations;
	}

	virtual TArray<FLiveLinkHubClientId> GetDiscoveredClients() const override
	{
		FReadScopeLock Locker(ClientsMapLock);
		TArray<FLiveLinkHubClientId> ClientAddresses;
		ClientsMap.GenerateKeyArray(ClientAddresses);
		return ClientAddresses;
	}

	virtual TOptional<FLiveLinkHubUEClientInfo> GetClientInfo(FLiveLinkHubClientId InAddress) const override
	{
		FReadScopeLock Locker(ClientsMapLock);
		TOptional<FLiveLinkHubUEClientInfo> ClientInfo;
		if (const FLiveLinkHubUEClientInfo* ClientInfoPtr = ClientsMap.Find(InAddress))
		{
			ClientInfo = *ClientInfoPtr;
		}

		return ClientInfo;
	}

	virtual FText GetClientDisplayName(FLiveLinkHubClientId InAddress) const override
	{
		FReadScopeLock Locker(ClientsMapLock);
		FText DisplayName;

		if (const FLiveLinkHubUEClientInfo* ClientInfoPtr = ClientsMap.Find(InAddress))
       	{
			DisplayName = FText::FromString(ClientInfoPtr->LongName);
       	}
       	else
       	{
       		DisplayName = LOCTEXT("InvalidSourceLabel", "Invalid Source");
       	}

       	return DisplayName;
	}

	virtual FOnClientEvent& OnClientEvent() override
	{
		return OnClientEventDelegate;
	}

	virtual FText GetClientStatus(FLiveLinkHubClientId Client) const override
	{
		FReadScopeLock Locker(ClientsMapLock);
		if (const FLiveLinkHubUEClientInfo* ClientInfoPtr = ClientsMap.Find(Client))
		{
			return StaticEnum<ELiveLinkClientStatus>()->GetDisplayNameTextByValue(static_cast<int64>(ClientInfoPtr->Status));
		}
		
		return LOCTEXT("InvalidStatus", "Disconnected");
	}

	/** Get whether a client should receive livelink data. */
	virtual bool IsClientEnabled(FLiveLinkHubClientId Client) const override
	{
		FReadScopeLock Locker(ClientsMapLock);
		if (const FLiveLinkHubUEClientInfo* ClientInfoPtr = ClientsMap.Find(Client))
		{
			return ClientInfoPtr->bEnabled;
		}
		return false;
	}

	virtual bool IsClientConnected(FLiveLinkHubClientId Client) const override
	{
		FReadScopeLock Locker(ClientsMapLock);
		if (const FLiveLinkHubUEClientInfo* ClientInfoPtr = ClientsMap.Find(Client))
		{
			return ClientInfoPtr->Status == ELiveLinkClientStatus::Connected;
		}
		return false;
	}

	/** Set whether a client should receive livelink data. */
	virtual void SetClientEnabled(FLiveLinkHubClientId Client, bool bInEnable) override
	{
		FWriteScopeLock Locker(ClientsMapLock);
		if (FLiveLinkHubUEClientInfo* ClientInfoPtr = ClientsMap.Find(Client))
		{
			ClientInfoPtr->bEnabled = bInEnable;
		}
	}

	/** Get whether a subject is enabled on a given client. */
	virtual bool IsSubjectEnabled(FLiveLinkHubClientId Client, const FLiveLinkSubjectKey& Subject) const override
	{
		FReadScopeLock Locker(ClientsMapLock);
		if (const FLiveLinkHubUEClientInfo* ClientInfoPtr = ClientsMap.Find(Client))
		{
			return !ClientInfoPtr->DisabledSubjects.Contains(Subject.SubjectName);
		}
		return false;
	}

	/** Set whether a subject should receive livelink data. */
	virtual void SetSubjectEnabled(FLiveLinkHubClientId Client, const FLiveLinkSubjectKey& Subject, bool bInEnable) override
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
	/** List of information we have on clients we have discovered. */
	TMap<FLiveLinkHubClientId, FLiveLinkHubUEClientInfo> ClientsMap;
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
