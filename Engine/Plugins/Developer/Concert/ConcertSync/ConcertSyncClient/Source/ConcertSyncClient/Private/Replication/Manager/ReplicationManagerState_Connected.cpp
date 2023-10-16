// Copyright Epic Games, Inc. All Rights Reserved.

#include "ReplicationManagerState_Connected.h"

#include "ConcertLogGlobal.h"
#include "ConcertTransportMessages.h"
#include "IConcertSession.h"
#include "ReplicationManagerState_Disconnected.h"
#include "Replication/ChangeStreamSharedUtils.h"
#include "Replication/Formats/FullObjectFormat.h"
#include "Replication/Messages/Handshake.h"
#include "Replication/Processing/ClientReplicationDataCollector.h"
#include "Replication/Processing/ObjectReplicationApplierProcessor.h"
#include "Replication/Processing/ObjectReplicationReceiver.h"
#include "Replication/Processing/ObjectReplicationSender.h"

#include "Algo/RemoveIf.h"
#include "Misc/ScopeExit.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

namespace UE::ConcertSyncClient::Replication
{
	FReplicationManagerState_Connected::FReplicationManagerState_Connected(
		TSharedRef<IConcertClientSession> LiveSession,
		IConcertClientReplicationBridge* ReplicationBridge,
		TArray<FReplicationStreamDescription> StreamDescriptions,
		FReplicationManager& Owner
		)
		: FReplicationManagerState(Owner)
		, LiveSession(LiveSession)
		, ReplicationBridge(ReplicationBridge)
		, RegisteredStreams(MoveTemp(StreamDescriptions))
		// TODO DP: Use config to determine which replication format to use
		, ReplicationFormat(MakeShared<ConcertSyncCore::FFullObjectFormat>())
		, ReplicationDataSource(MakeShared<FClientReplicationDataCollector>(
			ReplicationBridge,
			ReplicationFormat,
			FClientReplicationDataCollector::FGetClientStreams::CreateLambda([this]()
			{
				return &RegisteredStreams;
			}))
			)
		, Sender(MakeShared<ConcertSyncCore::FObjectReplicationSender>(LiveSession->GetSessionServerEndpointId(), LiveSession, ReplicationDataSource))
		, ReceivedDataCache(MakeShared<ConcertSyncCore::FObjectReplicationCache>(ReplicationFormat))
		, Receiver(MakeShared<ConcertSyncCore::FObjectReplicationReceiver>(LiveSession, ReceivedDataCache))
		, ReceivedReplicationQueuer(FClientReplicationDataQueuer::Make(ReplicationBridge, ReceivedDataCache))
		, ReplicationApplier(MakeShared<FObjectReplicationApplierProcessor>(ReplicationBridge, ReplicationFormat, ReceivedReplicationQueuer))
	{}

	FReplicationManagerState_Connected::~FReplicationManagerState_Connected()
	{
		// Technically not needed due to AddSP but let's be nice and clean up after ourselves
		LiveSession->OnTick().RemoveAll(this);
	}

	TFuture<FJoinReplicatedSessionResult> FReplicationManagerState_Connected::JoinReplicationSession(FJoinReplicatedSessionArgs Args)
	{
		UE_LOG(LogConcert, Warning, TEXT("JoinReplicationSession requested while already in a session"));
		return MakeFulfilledPromise<FJoinReplicatedSessionResult>(FJoinReplicatedSessionResult{ EJoinReplicationErrorCode::AlreadyInSession }).GetFuture();
	}

	void FReplicationManagerState_Connected::LeaveReplicationSession()
	{
		LiveSession->SendCustomEvent(FConcertReplication_LeaveEvent{}, LiveSession->GetSessionServerEndpointId(), EConcertMessageFlags::ReliableOrdered);
		ChangeState(MakeShared<FReplicationManagerState_Disconnected>(LiveSession, ReplicationBridge, GetOwner()));
	}

	IConcertClientReplicationManager::EStreamEnumerationResult FReplicationManagerState_Connected::ForEachRegisteredStream(
		TFunctionRef<EBreakBehavior(const FReplicationStreamDescription& Stream)> Callback
		) const
	{
		for (const FReplicationStreamDescription& Stream : RegisteredStreams)
		{
			if (Callback(Stream) == EBreakBehavior::Break)
			{
				break;
			}
		}
		return EStreamEnumerationResult::Iterated;
	}

	TFuture<FAuthorityChangeResponse> FReplicationManagerState_Connected::RequestAuthorityChange(FAuthorityChangeRequest Args)
	{
		// Stop replicating removed objects right now: the server will remove authority after processing this request.
		// At that point, it will log errors for receiving replication data from a client without authority.
		HandleReleasingReplicatedObjects(Args);
		
		return LiveSession->SendCustomRequest<FConcertReplication_ChangeAuthority_Request, FConcertReplication_ChangeAuthority_Response>(Args, LiveSession->GetSessionServerEndpointId())
			.Next([WeakThis = TWeakPtr<FReplicationManagerState_Connected>(SharedThis(this)), Args](FConcertReplication_ChangeAuthority_Response&& Response) mutable
			{
				if (const TSharedPtr<FReplicationManagerState_Connected> ThisPin = WeakThis.Pin())
				{
					ThisPin->UpdateReplicatedObjectsAfterAuthorityChange(MoveTemp(Args), Response);
				}

				return FAuthorityChangeResponse { MoveTemp(Response) };
			});
	}

	TFuture<FClientQueryResponse> FReplicationManagerState_Connected::QueryClientInfo(FClientQueryRequest Args)
	{
		if (EnumHasAllFlags(Args.QueryFlags, EConcertQueryClientStreamFlags::SkipAuthority | EConcertQueryClientStreamFlags::SkipStreamInfo))
		{
			UE_LOG(LogConcert, Warning, TEXT("Request QueryClientInfo is pointless because SkipAuthority and SkipStreamInfo are both set. Returning immediately..."));
			return MakeFulfilledPromise<FClientQueryResponse>().GetFuture();
		}
		
		return LiveSession->SendCustomRequest<FConcertReplication_QueryReplicationInfo_Request, FConcertReplication_QueryReplicationInfo_Response>(Args, LiveSession->GetSessionServerEndpointId())
			.Next([](FConcertReplication_QueryReplicationInfo_Response&& Response)
			{
				return FClientQueryResponse { MoveTemp(Response) };
			});
	}

	TFuture<FChangeStreamResponse> FReplicationManagerState_Connected::ChangeStream(FChangeStreamRequest Args)
	{
		// Stop replicating removed objects right now: the server will remove authority after processing this request.
		// At that point, it will log errors for receiving replication data from a client without authority.
		HandleRemovingReplicatedObjects(Args);
		
		return LiveSession->SendCustomRequest<FConcertReplication_ChangeStream_Request, FConcertReplication_ChangeStream_Response>(Args, LiveSession->GetSessionServerEndpointId())
			.Next([WeakThis = TWeakPtr<FReplicationManagerState_Connected>(SharedThis(this)), Args](FConcertReplication_ChangeStream_Response&& Response)
			{
				const TSharedPtr<FReplicationManagerState_Connected> ThisPin = WeakThis.Pin();
				if (ThisPin && Response.IsSuccess())
				{
					ThisPin->UpdateReplicatedObjectsAfterStreamChange(Args, Response);
				}
				
				return FChangeStreamResponse { MoveTemp(Response) };
			});
	}

	IConcertClientReplicationManager::EAuthorityEnumerationResult FReplicationManagerState_Connected::ForEachClientOwnedObject(
		TFunctionRef<EBreakBehavior(const FSoftObjectPath& Object, TSet<FGuid>&& OwningStreams)> Callback
		) const
	{
		TSet<FGuid> Result;
		int32 ExpectedNumStreams = 0;
		ReplicationDataSource->ForEachOwnedObject([this, &Callback, &Result, &ExpectedNumStreams](const FSoftObjectPath& ObjectPath)
		{
			// Reuse TSet (if possible) for a slightly better memory footprint
			ExpectedNumStreams = FMath::Max(ExpectedNumStreams, Result.Num());
			Result.Empty(Result.Num());
			
			ReplicationDataSource->AppendOwningStreamsForObject(ObjectPath, Result);
			return Callback(ObjectPath, MoveTemp(Result));
		});
		return EAuthorityEnumerationResult::Iterated;
	}

	TSet<FGuid> FReplicationManagerState_Connected::GetClientOwnedStreamsForObject(
		const FSoftObjectPath& ObjectPath
		) const
	{
		TSet<FGuid> Result;
		ReplicationDataSource->AppendOwningStreamsForObject(ObjectPath, Result);
		return Result;
	}

	void FReplicationManagerState_Connected::OnEnterState()
	{
		LiveSession->OnTick().AddSP(this, &FReplicationManagerState_Connected::Tick);
	}

	void FReplicationManagerState_Connected::Tick(IConcertClientSession& Session, float DeltaTime)
	{
		// TODO DP: Set this up in a config file
		constexpr double TimeBudget = 1.0 / 60.0;
		double TimeLeft = TimeBudget;

		auto NextIndex = [this](int32 Current){ return AlternatingTickTasks.IsValidIndex(Current + 1) ? Current + 1 : 0; };
		int32 CurrentIndex = NextTickTaskIndex;
		const double StartTime = FPlatformTime::Seconds();
		do
		{
			FTickTask Task = AlternatingTickTasks[CurrentIndex]; 
			Invoke(Task, this, TimeLeft);

			const double TotalElapsedTime = FPlatformTime::Seconds() - StartTime;
			TimeLeft = TimeBudget - TotalElapsedTime;
			CurrentIndex = NextIndex(CurrentIndex);
		}
		while (TimeLeft > 0.0 && CurrentIndex != NextTickTaskIndex);
		NextTickTaskIndex = CurrentIndex;
	}

	void FReplicationManagerState_Connected::TickSender(float TimeBudget)
	{
		Sender->ProcessObjects(TimeBudget);
	}

	void FReplicationManagerState_Connected::TickReceiver(float TimeBudget)
	{
		ReplicationApplier->ProcessObjects(TimeBudget);
	}

	void FReplicationManagerState_Connected::UpdateReplicatedObjectsAfterStreamChange(const FChangeStreamRequest& Request, const FConcertReplication_ChangeStream_Response& Response)
	{
		OnPreStreamsChangedDelegate.Broadcast(Request, { Response });
		ON_SCOPE_EXIT{ OnPostStreamsChangedDelegate.Broadcast(); };
		
		// Build RegisteredStreams while RegisteredStreams has the old, unupdated state
		TMap<FSoftObjectPath, TArray<FGuid>> BundledModifiedObjects;
		for (const TPair<FObjectInStreamID, FConcertReplication_ChangeStream_PutObject>& PutObject : Request.ObjectsToPut)
		{
			const FObjectInStreamID ObjectInfo = PutObject.Key;
			const FSoftObjectPath Object = ObjectInfo.Object;
			const FGuid StreamId = ObjectInfo.StreamId;
			
			const FReplicationStreamDescription* StreamDescription = RegisteredStreams.FindByPredicate([&StreamId](const FReplicationStreamDescription& Stream)
			{
				return Stream.BaseDescription.Identifier == StreamId;
			});
			// ReplicationDataSource only cares about inflight objects.
			// Newly added objects inflight because the client must first request authority.
			const bool bWasAddedByRequest = !ensure(StreamDescription) || !StreamDescription->BaseDescription.ReplicationMap.ReplicatedObjects.Contains(Object);
			if (!bWasAddedByRequest)
			{
				BundledModifiedObjects.FindOrAdd(Object).Add(StreamId);
			}
		}

		// The local cache must be updated before calling OnObjectStreamModified
		ConcertSyncCore::Replication::ChangeStreamUtils::ApplyValidatedRequest(Request, RegisteredStreams);
		
		for (const TPair<FSoftObjectPath, TArray<FGuid>>& ModifiedObject : BundledModifiedObjects)
		{
			ReplicationDataSource->OnObjectStreamModified(ModifiedObject.Key, ModifiedObject.Value);
		}
	}

	void FReplicationManagerState_Connected::HandleRemovingReplicatedObjects(const FChangeStreamRequest& Request) const
	{
		TMap<FSoftObjectPath, TArray<FGuid>> BundledRemovedObjects;
		for (const FObjectInStreamID& RemovedObject : Request.ObjectsToRemove)
		{
			BundledRemovedObjects.FindOrAdd(RemovedObject.Object).Add(RemovedObject.StreamId);
		}
		for (const TPair<FSoftObjectPath, TArray<FGuid>>& RemovedObjectInfo : BundledRemovedObjects)
		{
			// Removing an object from a stream implies removing its authority so stop replicating it
			ReplicationDataSource->RemoveReplicatedObjectStreams(RemovedObjectInfo.Key, RemovedObjectInfo.Value);
		}
	}

	void FReplicationManagerState_Connected::UpdateReplicatedObjectsAfterAuthorityChange(FAuthorityChangeRequest&& Request, const FConcertReplication_ChangeAuthority_Response& Response) const
	{
		OnPreAuthorityChangedDelegate.Broadcast(Request, { Response });
		ON_SCOPE_EXIT{ OnPostAuthorityChangedDelegate.Broadcast(); };
		
		for (TPair<FSoftObjectPath, FConcertStreamArray>& TakeAuthority : Request.TakeAuthority)
		{
			const FSoftObjectPath& ReplicatedObject = TakeAuthority.Key;
			// Request will be discarded so ...
			FConcertStreamArray& ReplicatedStreams = TakeAuthority.Value;

			const FConcertStreamArray* RejectedStreams = Response.RejectedObjects.Find(ReplicatedObject);
			if (RejectedStreams)
			{
				// ... reuse the memory
				ReplicatedStreams.StreamIds.SetNum(Algo::RemoveIf(ReplicatedStreams.StreamIds, [RejectedStreams](const FGuid& Stream)
				{
					return RejectedStreams->StreamIds.Contains(Stream);
				}));
			}

			const bool bWasFullyRejected = ReplicatedStreams.StreamIds.IsEmpty(); 
			if (!bWasFullyRejected)
			{
				ReplicationDataSource->AddReplicatedObjectStreams(TakeAuthority.Key, ReplicatedStreams.StreamIds);
			}
		}
	}

	void FReplicationManagerState_Connected::HandleReleasingReplicatedObjects(const FAuthorityChangeRequest& Request) const
	{
		for (const TPair<FSoftObjectPath, FConcertStreamArray>& ReleaseAuthority : Request.ReleaseAuthority)
		{
			ReplicationDataSource->RemoveReplicatedObjectStreams(ReleaseAuthority.Key, ReleaseAuthority.Value.StreamIds);
		}
	}
}

