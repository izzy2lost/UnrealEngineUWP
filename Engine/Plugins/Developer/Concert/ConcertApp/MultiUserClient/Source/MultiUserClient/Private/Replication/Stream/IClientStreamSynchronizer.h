// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Replication/Data/ObjectIds.h"
#include "Replication/Messages/ChangeStream.h"

#include "Async/Future.h"
#include "Replication/IConcertClientReplicationManager.h"

struct FObjectReplicationMap;

namespace UE::ConcertSyncClient::Replication
{
	struct FChangeStreamRequest;
	struct FChangeStreamResponse;
}

namespace UE::MultiUserClient
{
	struct FStreamChangelist
	{
		TSet<FObjectInStreamID> ObjectsToRemove;
		TMap<FObjectInStreamID, FConcertReplication_ChangeStream_PutObject> ObjectsToPut;
	};
	
	enum class ESubmitChangesErrorCode
	{
		/** Changes were accepted by the server */
		Success,
		/** Skipped because another request is in progress. */
		CannotSendRequest
	};

	struct FCompletedChangeSubmission
	{
		ConcertSyncClient::Replication::FChangeStreamRequest Request;
		ConcertSyncClient::Replication::FChangeStreamResponse Response;
	};

	struct FSubmitChangesResult
	{
		/** Error code of the submission. Determines whether Response is valid. */
		ESubmitChangesErrorCode ErrorCode;

		/** Valid if ErrorCode != ESubmitChangesErrorCode::AlreadyInProgress. */
		TOptional<FCompletedChangeSubmission> SubmissionInfo;
	};
	
	/**
	 * Keeps track of a client's registered streams.
	 * Exposes ways for observing and mutating streams.
	 *
	 * The implementation varies depending on whether the client corresponds to the local or a remote editor.
	 */
	class IClientStreamSynchronizer
	{
	public:

		/**
		 * Requests to change the stream. The future executes once the changes have been accepted on the server.
		 * @note This future can execute on any thread.
		 */
		virtual TFuture<FSubmitChangesResult> SubmitChanges(const FStreamChangelist& Changelist) = 0;
		/** @return Whether it's currently valid to call SubmitChanges. For example, it is not valid to call while there is already request in progress. */
		virtual bool CanMakeSubmitRequest() const = 0;

		/** @return The managed client's stream ID. */
		virtual FGuid GetStreamId() const = 0;
		/** @return What local instance thinks the client's server state is. */
		virtual const FObjectReplicationMap& GetServerState() const = 0;

		DECLARE_MULTICAST_DELEGATE_TwoParams(FOnChangesAccepted, const FObjectReplicationMap& OldServerState, const ConcertSyncClient::Replication::FChangeStreamRequest& AcceptedRequest);
		/** @return Event executed when a change request that was in transit was accepted by the server. This is executed before OnServerStateSynched: the local confirmed state is not updated, yet. */
		virtual FOnChangesAccepted& OnChangesAccepted() = 0;
		DECLARE_MULTICAST_DELEGATE(FOnServerStateChanged);
		/** @return Event executed when the result of GetServerState has been synched. Called after OnChangesAccepted_AnyThread. */
		virtual FOnServerStateChanged& OnServerStateChanged() = 0;
		
		virtual ~IClientStreamSynchronizer() = default;
	};
}
