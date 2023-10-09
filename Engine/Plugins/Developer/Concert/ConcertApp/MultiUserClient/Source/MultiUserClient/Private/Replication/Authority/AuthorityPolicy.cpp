// Copyright Epic Games, Inc. All Rights Reserved.

#include "AuthorityPolicy.h"

#include "Replication/Stream/LocalStreamChangeTracker.h"

#include "IConcertSyncClient.h"

#include "Templates/Function.h"

namespace UE::MultiUserClient
{
	namespace Private
	{
		static void ForEachObjectAddedByRequest(
			const FObjectReplicationMap& OldState,
			const ConcertSyncClient::Replication::FChangeStreamRequest& AcceptedRequest,
			TFunctionRef<void(const FObjectInStreamID& Object)> Callback
			)
		{
			// Call for every added stream
			for (const FReplicationStreamDescription_NetPacked& AddedStream : AcceptedRequest.StreamsToAdd)
			{
				for (const TPair<FSoftObjectPath, FReplicatedObjectInfo>& Pair : AddedStream.BaseDescription.ReplicationMap.ReplicatedObjects)
				{
					Callback({ AddedStream.BaseDescription.Identifier , Pair.Key });
				}
			}

			// Call for put object requests 
			for (const TPair<FObjectInStreamID, FConcertReplication_ChangeStream_PutObject>& Pair : AcceptedRequest.ObjectsToPut)
			{
				const bool bObjectWasAddedWithPut = !OldState.ReplicatedObjects.Contains(Pair.Key.Object);
				if (bObjectWasAddedWithPut)
				{
					Callback(Pair.Key);
				}
			}
		}
	}
	
	FAuthorityPolicy::FAuthorityPolicy(TSharedRef<IConcertSyncClient> InClient, TSharedRef<IClientStreamSynchronizer> InStreamSynchronizer)
		: Client(MoveTemp(InClient))
		, StreamSynchronizer(MoveTemp(InStreamSynchronizer))
	{
		// After submitting new objects, immediately request authority over it.
		StreamSynchronizer->OnChangesAccepted_AnyThread().AddRaw(this, &FAuthorityPolicy::OnChangesAccepted_AnyThread);
	}

	FAuthorityPolicy::~FAuthorityPolicy()
	{
		StreamSynchronizer->OnChangesAccepted_AnyThread().RemoveAll(this);
	}

	void FAuthorityPolicy::OnChangesAccepted_AnyThread(
		const FObjectReplicationMap& OldState,
		const ConcertSyncClient::Replication::FChangeStreamRequest& AcceptedRequest
		)
	{
		IConcertClientReplicationManager* ReplicationManager = Client->GetReplicationManager();
		if (!ensure(ReplicationManager))
		{
			return;
		}

		using namespace ConcertSyncClient::Replication;
		FAuthorityChangeRequest AuthorityChangeRequest;
		Private::ForEachObjectAddedByRequest(OldState, AcceptedRequest, [&AuthorityChangeRequest](const FObjectInStreamID& Object)
		{
			AuthorityChangeRequest.TakeAuthority.FindOrAdd(Object.Object).StreamIds.AddUnique(Object.StreamId);
		});
		
		OnAuthorityRequestSentDelegate.Broadcast(AuthorityChangeRequest);
		ReplicationManager->RequestAuthorityChange(AuthorityChangeRequest)
			.Next([this, DestructionDetection = LifetimeToken->AsWeak(), AuthorityChangeRequest](FAuthorityChangeResponse&& Response)
			{
				if (DestructionDetection.IsValid())
				{
					OnAuthorityResponseReceivedDelegate.Broadcast(AuthorityChangeRequest, Response);
				}
			});
	}
}
