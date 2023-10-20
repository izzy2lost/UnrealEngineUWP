// Copyright Epic Games, Inc. All Rights Reserved.

#include "ReplicationClientManager.h"

#include "IConcertSyncClient.h"
#include "LocalReplicationClient.h"
#include "RemoteReplicationClient.h"
#include "Assets/MultiUserReplicationSessionPreset.h"
#include "Replication/Stream/StreamSynchronizer_LocalClient.h"

#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

namespace UE::MultiUserClient
{
	FReplicationClientManager::FReplicationClientManager(
		TSharedRef<IConcertSyncClient> InClient,
		TSharedRef<IConcertClientSession> InSession
		)
		: SessionContent(NewObject<UMultiUserReplicationSessionPreset>(GetTransientPackage(), NAME_None, RF_Transient))
		, Session(InSession)
		, QueryService(InClient)
		, LocalClient([this, InClient]()
		{
			UMultiUserReplicationClientPreset* ClientPreset = SessionContent->AddClient();
			return FLocalReplicationClient(*ClientPreset, MakeUnique<FStreamSynchronizer_LocalClient>(InClient, ClientPreset->Stream->StreamId), InClient);
		}())
	{
		InSession->OnSessionClientChanged().AddRaw(this, &FReplicationClientManager::OnSessionClientChanged);

		for (const FGuid& ClientEndpointId : InSession->GetSessionClientEndpointIds())
		{
			// Nobody is subscribed at this point, so no need to broadcast
			constexpr bool bBroadcast = false;
			CreateRemoteClient(ClientEndpointId, bBroadcast);
		}
	}

	FReplicationClientManager::~FReplicationClientManager()
	{
		if (const TSharedPtr<IConcertClientSession> SessionPin = Session.Pin())
		{
			SessionPin->OnSessionClientChanged().RemoveAll(this);
		}
	}

	const FRemoteReplicationClient* FReplicationClientManager::FindRemoteClient(const FGuid& EndpointId) const
	{
		return RemoteClients.FindByPredicate([&EndpointId](const FRemoteReplicationClient& Client)
		{
			return Client.GetRemoteEndpointId() == EndpointId;
		});
	}

	void FReplicationClientManager::AddReferencedObjects(FReferenceCollector& Collector)
	{
		Collector.AddReferencedObject(SessionContent);
	}

	void FReplicationClientManager::OnSessionClientChanged(IConcertClientSession&, EConcertClientStatus NewStatus, const FConcertSessionClientInfo& ClientInfo)
	{
		const FGuid& ClientEndpointId = ClientInfo.ClientEndpointId;
		switch (NewStatus)
		{
			
		case EConcertClientStatus::Connected:
			CreateRemoteClient(ClientEndpointId);
			break;
			
		case EConcertClientStatus::Disconnected:
			{
				const int32 Index = RemoteClients.IndexOfByPredicate(
					[&ClientEndpointId](const FRemoteReplicationClient& Client)
					{
						return Client.GetRemoteEndpointId() == ClientEndpointId;
					});
				if (!ensure(RemoteClients.IsValidIndex(Index)))
				{
					return;
				}
				
				const FRemoteReplicationClient& Client = RemoteClients[Index];
				SessionContent->RemoveClient(*Client.GetClientContent());
				RemoteClients.RemoveAtSwap(Index);
				OnRemoteClientsChangedDelegate.Broadcast();
			}
			break;
			
		case EConcertClientStatus::Updated:
			break;
		default: checkNoEntry();
		}
	}
	
	void FReplicationClientManager::CreateRemoteClient(const FGuid& ClientEndpointId, bool bBroadcastDelegate)
	{
		RemoteClients.Emplace(*SessionContent->AddClient(), ClientEndpointId, QueryService);

		if (bBroadcastDelegate)
		{
			OnRemoteClientsChangedDelegate.Broadcast();
		}
	}
}
