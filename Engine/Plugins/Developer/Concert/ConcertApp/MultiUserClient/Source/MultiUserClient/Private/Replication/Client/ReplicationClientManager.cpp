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
		, AuthorityCache(*this)
		, LocalClient([this, InClient]()
		{
			UMultiUserReplicationClientPreset* ClientPreset = SessionContent->AddClient();
			return FLocalReplicationClient(*ClientPreset, MakeUnique<FStreamSynchronizer_LocalClient>(InClient, ClientPreset->Stream->StreamId), InClient, AuthorityCache);
		}())
		, SubmissionNotifier(*this)
	{
		AuthorityCache.RegisterEvents();
		InSession->OnSessionClientChanged().AddRaw(this, &FReplicationClientManager::OnSessionClientChanged);

		for (const FGuid& ClientEndpointId : InSession->GetSessionClientEndpointIds())
		{
			CreateRemoteClient(ClientEndpointId);
		}
	}

	FReplicationClientManager::~FReplicationClientManager()
	{
		if (const TSharedPtr<IConcertClientSession> SessionPin = Session.Pin())
		{
			SessionPin->OnSessionClientChanged().RemoveAll(this);
		}
	}

	TArray<TNonNullPtr<FRemoteReplicationClient>> FReplicationClientManager::GetRemoteClients() const
	{
		TArray<TNonNullPtr<FRemoteReplicationClient>> Result;
		Algo::Transform(RemoteClients, Result, [](const TUniquePtr<FRemoteReplicationClient>& Client) -> TNonNullPtr<FRemoteReplicationClient>
		{
			return Client.Get();
		});
		return Result;
	}

	const FRemoteReplicationClient* FReplicationClientManager::FindRemoteClient(const FGuid& EndpointId) const
	{
		const TUniquePtr<FRemoteReplicationClient>* Client = RemoteClients.FindByPredicate([&EndpointId](const TUniquePtr<FRemoteReplicationClient>& Client)
		{
			return Client->GetEndpointId() == EndpointId;
		});
		return Client ? Client->Get() : nullptr;
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
					[&ClientEndpointId](const TUniquePtr<FRemoteReplicationClient>& Client)
					{
						return Client->GetEndpointId() == ClientEndpointId;
					});
				if (!ensure(RemoteClients.IsValidIndex(Index)))
				{
					return;
				}

				{
					const TUniquePtr<FRemoteReplicationClient> Client = MoveTemp(RemoteClients[Index]);
					OnPreRemoteClientRemovedDelegate.Broadcast(*Client.Get());
					SessionContent->RemoveClient(*Client->GetClientContent());
					RemoteClients.RemoveAtSwap(Index);
				}
				// We want to broadcast after the client has been fully cleaned up
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
		TUniquePtr<FRemoteReplicationClient> RemoteClientPtr = MakeUnique<FRemoteReplicationClient>(ClientEndpointId, *SessionContent->AddClient(), QueryService);
		FRemoteReplicationClient& RemoteClient = *RemoteClientPtr;
		RemoteClients.Emplace(
			MoveTemp(RemoteClientPtr)
			);

		if (bBroadcastDelegate)
		{
			OnPostRemoteClientAddedDelegate.Broadcast(RemoteClient);
			OnRemoteClientsChangedDelegate.Broadcast();
		}
	}
}
