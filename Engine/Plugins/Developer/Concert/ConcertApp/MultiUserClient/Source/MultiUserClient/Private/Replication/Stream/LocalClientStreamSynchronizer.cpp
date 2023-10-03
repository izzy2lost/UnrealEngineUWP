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
		const FGuid& InLocalClientStreamId,
		TAttribute<FObjectReplicationMap*> InStreamWithInProgressChangesAttribute,
		FOnModifyReplicationMap InOnModifyReplicationMapDelegate
		)
		: LocalClient(MoveTemp(InLocalClient))
		, LocalClientStreamId(InLocalClientStreamId)
		, StreamWithInProgressChangesAttribute(MoveTemp(InStreamWithInProgressChangesAttribute))
		, OnModifyReplicationMapDelegate(MoveTemp(InOnModifyReplicationMapDelegate))
	{
		check(InStreamWithInProgressChangesAttribute.IsBound() || InStreamWithInProgressChangesAttribute.IsSet());
	}

	void FLocalClientStreamSynchronizer::RefreshChangesCache()
	{
		CachedDeltaChange = DiffChanges();
		RefreshWarnings();
	}

	TFuture<FLocalClientStreamSynchronizer::FSubmissionResult> FLocalClientStreamSynchronizer::SubmitChanges(bool bRefreshChanges)
	{
		using namespace ConcertSyncClient::Replication;

		IConcertClientReplicationManager* ReplicationManager = LocalClient->GetReplicationManager();
		if (!ensure(ReplicationManager) || !CanSubmitChanges())
		{
			return MakeFulfilledPromise<FSubmissionResult>(FSubmissionResult{}).GetFuture();
		}

		if (bRefreshChanges)
		{
			RefreshChangesCache();
		}
		ChangeInTransit = CachedDeltaChange;
		FChangeStreamRequest Request = BuildChangeRequest();
		
		return ReplicationManager->ChangeStream(Request)
			.Next([this, DestructionDetection = LifetimeToken->AsWeak(), Request](FChangeStreamResponse&& Response)
			{
				const FSubmissionResult SubmissionResult { Request, Response };
				// The request might execute after we're destroyed, e.g. by leaving session while request is on the way.
				// In that case, the Concert session triggers the OnSessionConnectionChanged which destroys us. Only after that, all the requests are timed out.
				if (!DestructionDetection.IsValid())
				{
					return SubmissionResult;
				}

				const FChangelist ChangeThatWasInTransit = MoveTemp(ChangeInTransit.GetValue());
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

	void FLocalClientStreamSynchronizer::RevertCachedChanges()
	{
		if (HasRevertableLocalChanges())
		{
			FScopedTransaction Transaction(LOCTEXT("RevertCachedChanges", "Revert replication changes"));
			OnModifyReplicationMapDelegate.ExecuteIfBound();
			
			*StreamWithInProgressChangesAttribute.Get() = ConfirmedServerState;
			CachedDeltaChange = {};

			check(IsInGameThread());
			OnChangesRevertedDelegate.Broadcast();
			RefreshChangesCache();
		}
	}

	bool FLocalClientStreamSynchronizer::HasSubmittableLocalChanges() const
	{
		return !CachedDeltaChange.ObjectsToPut.IsEmpty() || !CachedDeltaChange.ObjectsToRemove.IsEmpty();
	}

	bool FLocalClientStreamSynchronizer::HasRevertableLocalChanges() const
	{
		return HasSubmittableLocalChanges() || HasAnyWarnings();
	}

	bool FLocalClientStreamSynchronizer::IsChangeRequestInTransit() const
	{
		return ChangeInTransit.IsSet();
	}

	EObjectWarningFlags FLocalClientStreamSynchronizer::GetObjectWarningFlags(const FSoftObjectPath& ObjectPath)
	{
		const EObjectWarningFlags* Flags = ObjectsWithWarnings.Find(ObjectPath);
		return Flags ? *Flags : EObjectWarningFlags::Ok;
	}

	bool FLocalClientStreamSynchronizer::HasAnyWarnings() const
	{
		return !ObjectsWithWarnings.IsEmpty();
	}

	FLocalClientStreamSynchronizer::EObjectChangeType FLocalClientStreamSynchronizer::GetObjectChanges(const FSoftObjectPath& Object) const
	{
		// TODO DP:
		return EObjectChangeType::NoChange;
	}

	FLocalClientStreamSynchronizer::EPropertyChangeType FLocalClientStreamSynchronizer::GetPropertyChanges(const FSoftObjectPath& Object, const FConcertPropertyChain& PropertyChain) const
	{
		// TODO DP:
		return EPropertyChangeType::NoChange;
	}
	
	FLocalClientStreamSynchronizer::FChangelist FLocalClientStreamSynchronizer::DiffChanges(
		const FGuid& StreamId,
		const FObjectReplicationMap& Base,
		const FObjectReplicationMap& Changed
		)
	{
		// BuildRequestFromDiff does not tolerate any invalid entries (like empty properties, which the UI generates right after you add an object to the list)
		FObjectReplicationMap Cleansed = Changed;
		ConcertSyncCore::Replication::ChangeStreamUtils::IterateInvalidEntries(Changed, [&Cleansed](const FSoftObjectPath& InvalidObject, const FReplicatedObjectInfo&)
		{
			Cleansed.ReplicatedObjects.Remove(InvalidObject);
			return EBreakBehavior::Continue;
		});

		FConcertChangeStream_Request Request = ConcertSyncCore::Replication::ChangeStreamUtils::BuildRequestFromDiff(StreamId, Base, Cleansed);
		return { MoveTemp(Request.ObjectsToRemove), MoveTemp(Request.ObjectsToPut) };
	}

	void FLocalClientStreamSynchronizer::RefreshWarnings()
	{
		ObjectsWithWarnings.Empty();
		const FObjectReplicationMap* Map = StreamWithInProgressChangesAttribute.Get();
		if (!Map)
		{
			return;
		}
		
		ConcertSyncCore::Replication::ChangeStreamUtils::IterateInvalidEntries(*Map, [this](const FSoftObjectPath& InvalidObject, const FReplicatedObjectInfo& Info)
		{
			EObjectWarningFlags Flags = EObjectWarningFlags::Ok;
			if (Info.PropertySelection.ReplicatedProperties.IsEmpty())
			{
				Flags |= EObjectWarningFlags::MissingProperties;
			}
			
			ObjectsWithWarnings.Add(InvalidObject, Flags);
			return EBreakBehavior::Continue;
		});
	}

	ConcertSyncClient::Replication::FChangeStreamRequest FLocalClientStreamSynchronizer::BuildChangeRequest() const
	{
		switch (ComputeNextRequestType())
		{
		case EChangeRequestType::CreateNewStream: return BuildChangeRequest_CreateNewStream(LocalClientStreamId, CachedDeltaChange);
		case EChangeRequestType::UpdateExistingStream: return BuildChangeRequest_UpdateExistingStream(CachedDeltaChange);
		default: checkNoEntry(); return {};
		}
	}

	ConcertSyncClient::Replication::FChangeStreamRequest FLocalClientStreamSynchronizer::BuildChangeRequest_CreateNewStream(const FGuid& StreamId, const FChangelist& FromChangelist)
	{
		const TMap<FObjectInStreamID, FConcertChangeStream_PutObject>& ObjectsToPut = FromChangelist.ObjectsToPut;
		
		ConcertSyncClient::Replication::FChangeStreamRequest Request;
		Request.StreamsToAdd.Emplace();
		FReplicationStreamDescription_NetPacked& NewStream = Request.StreamsToAdd[0];
		NewStream.BaseDescription.Identifier = StreamId;
			
		// If creating a new stream, the objects must be supplied in the description instead of in PutObjects!
		FObjectReplicationMap& ReplicationMap = NewStream.BaseDescription.ReplicationMap;
		ReplicationMap.ReplicatedObjects.Reserve(ObjectsToPut.Num());
		for (const TPair<FObjectInStreamID, FConcertChangeStream_PutObject>& PutObjectPair : ObjectsToPut)
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

	ConcertSyncClient::Replication::FChangeStreamRequest FLocalClientStreamSynchronizer::BuildChangeRequest_UpdateExistingStream(FChangelist FromChangelist)
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
		const FChangelist& ChangeThatWasInTransit
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
					
		RefreshChangesCache();
	}
}

#undef LOCTEXT_NAMESPACE