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
#include "LiveLinkHubMessages.h"
#include "LiveLinkProviderImpl.h"
#include "LiveLinkSettings.h"
#include "MessageEndpointBuilder.h"
#include "MessageHandlers.h"
#include "Misc/ScopeLock.h"
#include "TimerManager.h"

/** 
 * LiveLink Provider that allows getting more information about a UE client by communicating with a LiveLinkHub MessageBus Source.
 */
class FLiveLinkHubProvider : public FLiveLinkProvider, public ILiveLinkHubClientsModel, public TSharedFromThis<FLiveLinkHubProvider>
{
public:
	/**
	 * Create a message bus handler that will dispatch messages on the game thread. 
	 * This is useful to receive some messages on AnyThread and delegate others on the game thread (ie. for methods that will trigger UI updates which need to happen on game thread. )
	 */
	template <typename MessageType>
	TSharedRef<TGameThreadMessageHandler<MessageType, FLiveLinkHubProvider>> MakeHandler(TGameThreadMessageHandler<MessageType, FLiveLinkHubProvider>::FuncType Func)
	{
		return MakeShared<TGameThreadMessageHandler<MessageType, FLiveLinkHubProvider>>(this, Func);
	}

	FLiveLinkHubProvider()
		: FLiveLinkProvider(TEXT("LiveLink Hub"), false)
	{
		Annotations.Add(FLiveLinkHubMessageAnnotation::ProviderTypeAnnotation, UE::LiveLinkHub::Private::LiveLinkHubProviderType.ToString());

		FMessageEndpointBuilder EndpointBuilder = FMessageEndpoint::Builder(*GetProviderName());
		EndpointBuilder.WithHandler(MakeHandler<FLiveLinkClientInfoMessage>(&FLiveLinkHubProvider::HandleClientInfoMessage));
		EndpointBuilder.WithHandler(MakeHandler<FLiveLinkHubConnectMessage>(&FLiveLinkHubProvider::HandleHubConnectMessage));

		CreateMessageEndpoint(EndpointBuilder);

		const double ValidateConnectionsRate = GetDefault<ULiveLinkSettings>()->MessageBusPingRequestFrequency;
		GEditor->GetTimerManager()->SetTimer(ValidateConnectionsTimer, FTimerDelegate::CreateRaw(this, &FLiveLinkHubProvider::ValidateConnections), ValidateConnectionsRate, true);
	}

	virtual ~FLiveLinkHubProvider()
	{
		if (GEditor)
		{
			GEditor->GetTimerManager()->ClearTimer(ValidateConnectionsTimer);
		}
	}

private:
	/** Handle a connection message resulting from a livelink hub message bus source connecting to this provider. */
	void HandleHubConnectMessage(const FLiveLinkHubConnectMessage& Message, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context)
	{
		FLiveLinkConnectMessage ConnectMessage;
		ConnectMessage.LiveLinkVersion = Message.ClientInfo.LiveLinkVersion;
		FLiveLinkProvider::HandleConnectMessage(ConnectMessage, Context);

		FMessageAddress ConnectionAddress = Context->GetSender();

		// Remove old entries if one is found
		TOptional<FMessageAddress> RemovedAddress;
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
		// todo: If we want to show disconnected clients, we should update the status in the clients map rather than remove it.
		for (const FMessageAddress& TrackedAddress : ClosedAddresses)
		{
			if (ClientsMap.Contains(TrackedAddress))
			{
				ClientsMap.Remove(TrackedAddress);
				OnClientEventDelegate.Broadcast(TrackedAddress, EClientEventType::Modified);
			}
		}
	}

	virtual TMap<FName, FString> GetAnnotations() const override
	{
		return Annotations;
	}

	virtual TArray<FMessageAddress> GetClients() const override
	{
		TArray<FMessageAddress> ClientAddresses;
		ClientsMap.GenerateKeyArray(ClientAddresses);
		return ClientAddresses;
	}

	TOptional<FLiveLinkHubUEClientInfo> GetClientInfo(const FMessageAddress& InAddress) const override
	{
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
};
