// Copyright Epic Games, Inc. All Rights Reserved.

#include "LocalClientStreamSynchronizer.h"

#include "IConcertSyncClient.h"
#include "Replication/IConcertClientReplicationManager.h"
#include "Replication/Messages/ChangeStream.h"

#define LOCTEXT_NAMESPACE "FLocalClientStreamDiffer"

namespace UE::MultiUserClient
{
	FLocalClientStreamSynchronizer::FLocalClientStreamSynchronizer(
		TSharedRef<IConcertSyncClient> InLocalClient,
		const FGuid& InLocalClientStreamId
		)
		: LocalClient(MoveTemp(InLocalClient))
		, LocalClientStreamId(InLocalClientStreamId)
	{
		IConcertClientReplicationManager* ReplicationManager = LocalClient->GetReplicationManager();
		if (ensure(ReplicationManager))
		{
			ReplicationManager->OnPreStreamsChanged().AddRaw(this, &FLocalClientStreamSynchronizer::OnPreStreamsChanged);
			ReplicationManager->OnPostStreamsChanged().AddRaw(this, &FLocalClientStreamSynchronizer::OnPostStreamsChanged);
		}
	}

	FLocalClientStreamSynchronizer::~FLocalClientStreamSynchronizer()
	{
		IConcertClientReplicationManager* ReplicationManager = LocalClient->GetReplicationManager();
		if (ReplicationManager)
		{
			ReplicationManager->OnPreStreamsChanged().RemoveAll(this);
			ReplicationManager->OnPostStreamsChanged().RemoveAll(this);
		}
	}

	TFuture<FSubmitChangesResult> FLocalClientStreamSynchronizer::SubmitChanges(const FStreamChangelist& Changelist)
	{
		using namespace ConcertSyncClient::Replication;

		IConcertClientReplicationManager* ReplicationManager = LocalClient->GetReplicationManager();
		if (!ensure(ReplicationManager) || !CanMakeSubmitRequest())
		{
			return MakeFulfilledPromise<FSubmitChangesResult>(FSubmitChangesResult{ ESubmitChangesErrorCode::CannotSendRequest }).GetFuture();
		}
		
		FChangeStreamRequest Request = BuildChangeRequest(Changelist);
		ChangeInTransit = Request;
		
		return ReplicationManager->ChangeStream(Request)
			.Next([this, DestructionDetection = LifetimeToken->AsWeak(), Request](FChangeStreamResponse&& Response)
			{
				const FSubmitChangesResult SubmissionResult { ESubmitChangesErrorCode::Success, { FCompletedChangeSubmission{Request, Response } } };
				// The request might execute after we're destroyed, e.g. by leaving session while request is on the way.
				// In that case, the Concert session triggers the OnSessionConnectionChanged which destroys us. Only after that, all the requests are timed out.
				if (DestructionDetection.IsValid())
				{
					// Already called by OnPreStreamsChanged but needed here, too, in case request fails.
					ChangeInTransit.Reset();
				}

				return SubmissionResult;
			});
	}

	const FObjectReplicationMap& FLocalClientStreamSynchronizer::GetServerState() const
	{
		IConcertClientReplicationManager* ReplicationManager = LocalClient->GetReplicationManager();
		if (!ensure(ReplicationManager))
		{
			return EmptyState;
		}
		
		const FObjectReplicationMap* Result = nullptr;
		ReplicationManager->ForEachRegisteredStream([this, &Result](const FReplicationStreamDescription& Stream)
		{
			if (Stream.BaseDescription.Identifier == LocalClientStreamId)
			{
				Result = &Stream.BaseDescription.ReplicationMap;
				return EBreakBehavior::Break;
			}
			return EBreakBehavior::Continue;
		});
		return Result
			? *Result
			: EmptyState;
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
		return GetServerState().ReplicatedObjects.IsEmpty()
			? EChangeRequestType::CreateNewStream
			: EChangeRequestType::UpdateExistingStream;
	}

	void FLocalClientStreamSynchronizer::OnPreStreamsChanged(
		const ConcertSyncClient::Replication::FChangeStreamRequest& ChangeStreamRequest,
		const ConcertSyncClient::Replication::FChangeStreamResponse& ChangeStreamResponse
		)
	{
		// Was the change made by us? External code can also call ChangeStream.
		const bool bRequestEqualToInTransit =  ChangeInTransit.IsSet() && *ChangeInTransit == ChangeStreamRequest;
		if (bRequestEqualToInTransit)
		{
			// Allows making another change in response to Broadcast
			ChangeInTransit.Reset();
			OnChangesAcceptedDelegate.Broadcast(GetServerState(), ChangeStreamRequest);
		}
	}

	void FLocalClientStreamSynchronizer::OnPostStreamsChanged()
	{
		// Note that this fires even for non MU-related changes, e.g. due to some other system's Concert replication API usage.
		OnServerStateChangedDelegate.Broadcast();
	}
}

#undef LOCTEXT_NAMESPACE