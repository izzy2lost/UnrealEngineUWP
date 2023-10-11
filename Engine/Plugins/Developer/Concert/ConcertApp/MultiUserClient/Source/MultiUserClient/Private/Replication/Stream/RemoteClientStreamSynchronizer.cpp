// Copyright Epic Games, Inc. All Rights Reserved.

#include "RemoteClientStreamSynchronizer.h"

#include "ConcertLogGlobal.h"
#include "Assets/MultiUserReplicationClientPreset.h"
#include "Replication/Util/RegularQueryService.h"

namespace UE::MultiUserClient
{
	constexpr float QueryTimeInterval = 1.f;
	
	FRemoteClientStreamSynchronizer::FRemoteClientStreamSynchronizer(const FGuid& RemoteEndpointId, FRegularQueryService& InQueryService)
		: QueryService(InQueryService)
		, QueryStreamHandle(
			QueryService.RegisterStreamQuery(
				RemoteEndpointId,
				FStreamQueryDelegate::CreateRaw(this, &FRemoteClientStreamSynchronizer::HandleStreamQuery)
				)
			)
	{}

	FRemoteClientStreamSynchronizer::~FRemoteClientStreamSynchronizer()
	{
		QueryService.UnregisterStreamQuery(QueryStreamHandle);
	}

	TFuture<FSubmitChangesResult> FRemoteClientStreamSynchronizer::SubmitChanges(const FStreamChangelist& Changelist)
	{
		
		// UE-180657: Changing remote client's stream is not implemented for now
		return MakeFulfilledPromise<FSubmitChangesResult>(FSubmitChangesResult{ ESubmitChangesErrorCode::CannotSendRequest }).GetFuture();
	}

	bool FRemoteClientStreamSynchronizer::CanMakeSubmitRequest() const
	{
		// UE-180657: Changing remote client's stream is not implemented for now
		return false;
	}

	FGuid FRemoteClientStreamSynchronizer::GetStreamId() const
	{
		return UMultiUserReplicationClientPreset::MultiUserStreamID;
	}

	void FRemoteClientStreamSynchronizer::HandleStreamQuery(const TArray<FSharedReplicationStreamDescription>& Streams)
	{
		// MU clients use UMultiUserReplicationClientPreset::MultiUserStreamID for streams.
		// That handles the (unlikely) case in which some external logic had added streams to the same client which we must differentiate
		const FSharedReplicationStreamDescription* MultiUserDescription = Streams.FindByPredicate([](const FSharedReplicationStreamDescription& Description)
		{
			return Description.Identifier == UMultiUserReplicationClientPreset::MultiUserStreamID;
		});
		if (MultiUserDescription
			// Avoid unnecessary Broadcast()s
			&& LastKnownServerState != MultiUserDescription->ReplicationMap)
		{
			LastKnownServerState = MultiUserDescription->ReplicationMap;
			OnServerStateSynchedDelegate.Broadcast();
		}
		// If the remote client removed the last object, the stream is implicitly deleted.
		else if (!MultiUserDescription && !LastKnownServerState.ReplicatedObjects.IsEmpty())
		{
			LastKnownServerState = {};
			OnServerStateSynchedDelegate.Broadcast();
		}
	}
}
