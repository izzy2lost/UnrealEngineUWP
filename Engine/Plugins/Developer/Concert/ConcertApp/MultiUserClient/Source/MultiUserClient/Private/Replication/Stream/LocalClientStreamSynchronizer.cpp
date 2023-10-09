// Copyright Epic Games, Inc. All Rights Reserved.

#include "LocalClientStreamSynchronizer.h"

#include "IConcertSyncClient.h"
#include "Replication/ChangeStreamSharedUtils.h"
#include "Replication/IConcertClientReplicationManager.h"

#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "FLocalClientStreamDiffer"

namespace UE::MultiUserClient
{
	FLocalClientStreamSynchronizer::FLocalClientStreamSynchronizer(
		TSharedRef<IConcertSyncClient> InLocalClient,
		const FGuid& InLocalClientStreamId
		)
		: LocalClient(MoveTemp(InLocalClient))
		, LocalClientStreamId(InLocalClientStreamId)
	{}

	TFuture<FSubmitChangesResult> FLocalClientStreamSynchronizer::SubmitChanges(const FStreamChangelist& Changelist)
	{
		using namespace ConcertSyncClient::Replication;

		IConcertClientReplicationManager* ReplicationManager = LocalClient->GetReplicationManager();
		if (!ensure(ReplicationManager) || !CanMakeSubmitRequest())
		{
			return MakeFulfilledPromise<FSubmitChangesResult>(FSubmitChangesResult{ ESubmitChangesErrorCode::CannotSendRequest }).GetFuture();
		}
		
		ChangeInTransit = Changelist;
		FChangeStreamRequest Request = BuildChangeRequest(Changelist);
		
		return ReplicationManager->ChangeStream(Request)
			.Next([this, DestructionDetection = LifetimeToken->AsWeak(), Request](FChangeStreamResponse&& Response)
			{
				const FSubmitChangesResult SubmissionResult { ESubmitChangesErrorCode::Success, { FCompletedChangeSubmission{Request, Response } } };
				// The request might execute after we're destroyed, e.g. by leaving session while request is on the way.
				// In that case, the Concert session triggers the OnSessionConnectionChanged which destroys us. Only after that, all the requests are timed out.
				if (!DestructionDetection.IsValid())
				{
					return SubmissionResult;
				}

				const FStreamChangelist ChangeThatWasInTransit = MoveTemp(ChangeInTransit.GetValue());
				ChangeInTransit.Reset();
				
				if (Response.IsSuccess())
				{
					// Tell everybody who cares old and new implicit state. They can e.g. decide to request authority for the new objects.
					OnChangesAcceptedDelegate.Broadcast(ConfirmedServerState, Request);
					UpdateConfirmedServerState(Request, ChangeThatWasInTransit);
				}
				
				return SubmissionResult;
			});
	}

	ConcertSyncClient::Replication::FChangeStreamRequest FLocalClientStreamSynchronizer::BuildChangeRequest(const FStreamChangelist& Changelist) const
	{
		switch (ComputeNextRequestType())
		{
		case EChangeRequestType::CreateNewStream: return BuildChangeRequest_CreateNewStream(LocalClientStreamId, Changelist);
		case EChangeRequestType::UpdateExistingStream: return BuildChangeRequest_UpdateExistingStream(Changelist);
		default: checkNoEntry(); return {};
		}
	}

	ConcertSyncClient::Replication::FChangeStreamRequest FLocalClientStreamSynchronizer::BuildChangeRequest_CreateNewStream(const FGuid& StreamId, const FStreamChangelist& FromChangelist)
	{
		const TMap<FObjectInStreamID, FConcertReplication_ChangeStream_PutObject>& ObjectsToPut = FromChangelist.ObjectsToPut;
		
		ConcertSyncClient::Replication::FChangeStreamRequest Request;
		Request.StreamsToAdd.Emplace();
		FReplicationStreamDescription_NetPacked& NewStream = Request.StreamsToAdd[0];
		NewStream.BaseDescription.Identifier = StreamId;
			
		// If creating a new stream, the objects must be supplied in the description instead of in PutObjects!
		FObjectReplicationMap& ReplicationMap = NewStream.BaseDescription.ReplicationMap;
		ReplicationMap.ReplicatedObjects.Reserve(ObjectsToPut.Num());
		for (const TPair<FObjectInStreamID, FConcertReplication_ChangeStream_PutObject>& PutObjectPair : ObjectsToPut)
		{
			const TOptional<FReplicatedObjectInfo> NewObjectInfo = PutObjectPair.Value.MakeObjectInfoIfValid();
			// The editing UI allows adding objects without properties (to make UX easier) - do not submit those to the server.
			if (!NewObjectInfo)
			{
				continue;
			}
			checkf(PutObjectPair.Key.StreamId == StreamId, TEXT("BuildChangeRequest should always use LocalClientStreamId ID!"));
				
			ReplicationMap.ReplicatedObjects.Add(PutObjectPair.Key.Object, *NewObjectInfo);
		}
		
		return Request;
	}

	ConcertSyncClient::Replication::FChangeStreamRequest FLocalClientStreamSynchronizer::BuildChangeRequest_UpdateExistingStream(FStreamChangelist FromChangelist)
	{
		return { MoveTemp(FromChangelist.ObjectsToRemove), MoveTemp(FromChangelist.ObjectsToPut) };
	}
	
	FLocalClientStreamSynchronizer::EChangeRequestType FLocalClientStreamSynchronizer::ComputeNextRequestType() const
	{
		return ConfirmedServerState.ReplicatedObjects.IsEmpty()
			? EChangeRequestType::CreateNewStream
			: EChangeRequestType::UpdateExistingStream;
	}

	void FLocalClientStreamSynchronizer::UpdateConfirmedServerState(
		const ConcertSyncClient::Replication::FChangeStreamRequest& Request,
		const FStreamChangelist& ChangeThatWasInTransit
		)
	{
		const bool bCreatedNewStream = !Request.StreamsToAdd.IsEmpty(); 
		if (bCreatedNewStream)
		{
			ConfirmedServerState = Request.StreamsToAdd[0].BaseDescription.ReplicationMap;
		}
		else
		{
			// The goal here is to leverage ApplyValidatedRequest for updating ConfirmedServerState.
			TArray<FReplicationStreamDescription> FakeDescriptions;
			FakeDescriptions.Emplace();
			FReplicationStreamDescription& FakeStreamDescription = FakeDescriptions[0];
			FakeStreamDescription.BaseDescription.Identifier = LocalClientStreamId;
			// Cheaply move ConfirmedServerState into the FReplicationStreamDescription so it satisfies the ApplyValidatedRequest API...
			FakeStreamDescription.BaseDescription.ReplicationMap = MoveTemp(ConfirmedServerState);
			ConcertSyncCore::Replication::ChangeStreamUtils::ApplyValidatedRequest(
				BuildChangeRequest_UpdateExistingStream(ChangeThatWasInTransit),
				FakeDescriptions
				);
						
			// Careful: ApplyValidatedRequest may have emptied the array!
			FObjectReplicationMap NewServerState = FakeDescriptions.IsEmpty()
				? FObjectReplicationMap{}
			: FakeStreamDescription.BaseDescription.ReplicationMap;
						
			// ... and cheaply move the updated map back into our ConfirmedServerState
			ConfirmedServerState = MoveTemp(NewServerState);
		}
					
		OnServerStateSynchedDelegate.Broadcast();
	}
}

#undef LOCTEXT_NAMESPACE