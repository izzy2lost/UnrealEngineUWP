// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/EBreakBehavior.h"
#include "Replication/Messages/ConcertReplicationHandshakeMessages.h"
#include "Replication/Data/ReplicationClientDescription.h"
#include "Replication/Messages/ConcertReplicationEvents.h"

template<typename ResultType>
class TFuture;

struct FReplicationClientDescription;
struct FReplicationStreamDescription;

namespace UE::ConcertSyncClient::Replication
{
	struct FJoinReplicatedSessionArgs
	{
		/** General info about this client, such as the type of data it wishes to receive. */
		FReplicationClientDescription ClientInfo;
		/** The streams this client offers. */
		TArray<FReplicationStreamDescription> Streams;
	};

	struct FJoinReplicatedSessionResult
	{
		/** Error code sent by the server. */
		EJoinReplicationErrorCode ErrorCode;
		/** Optional error message to help human user resolve the error. */
		FString DetailedErrorMessage;

		FJoinReplicatedSessionResult(EJoinReplicationErrorCode ErrorCode, FString DetailedErrorMessage = TEXT(""))
			: ErrorCode(ErrorCode)
			, DetailedErrorMessage(MoveTemp(DetailedErrorMessage))
		{}
	};

	// The intention here is to wrap the request in case there is some more specific meta data we want to add in the future.
	struct FAuthorityChangeRequest : FConcertChangeAuthority_Request {};
	struct FAuthorityChangeResponse : FConcertChangeAuthority_Response {};
	struct FClientQueryRequest : FConcertQueryReplicationInfo_Request {};
	struct FClientQueryResponse : FConcertQueryReplicationInfo_Response {};
	struct FChangeStreamRequest : FConcertChangeStream_Request {};
	struct FChangeStreamResponse : FConcertChangeStream_Response {};
}

/**
 * Handles all communication with the server regarding replication.
 * 
 * Keeps a list of properties to send along with their send rules.
 * Tells the server which properties this client is interested in receiving.
 */
class CONCERTSYNCCLIENT_API IConcertClientReplicationManager
{
public:

	/**
	 * Joins a replication session.
	 * Subsequent calls to JoinReplicationSession will fail until either the resulting TFuture returns or LeaveReplicationSession is called.
	 *
	 * @note The future may execute on any thread. Take care to synchronize correctly with the game thread if needed.
	 */
	virtual TFuture<UE::ConcertSyncClient::Replication::FJoinReplicatedSessionResult> JoinReplicationSession(UE::ConcertSyncClient::Replication::FJoinReplicatedSessionArgs Args) = 0;
	/** Leaves the current replication session. */
	virtual void LeaveReplicationSession() = 0;

	/**
	 * Whether it is valid to call JoinReplicationSession right now. Returns false if it was called and its future has not yet concluded.
	 * If this returns true, you can call JoinReplicationSession to attempt joining the session.
	 */
	virtual bool CanJoin() = 0;
	/** Whether JoinReplicationSession completed successfully and LeaveReplicationSession has not yet been called. */
	virtual bool IsConnectedToReplicationSession() = 0;

	enum class EStreamEnumerationResult { NoRegisteredStreams, Iterated };
	/**
	 * Iterates the streams the client has registered with the server.
	 * It only makes sense to call this function the manager has joined a replication session.
	 * @return Whether this manager is connected to a session (Iterated) or not (NoRegisteredStreams).
	 */
	virtual EStreamEnumerationResult ForEachRegisteredStream(TFunctionRef<EBreakBehavior(const FReplicationStreamDescription& Stream)> Callback) const = 0;
	/** @return Whether this manager is currently in a replication session (basically whether ForEachRegisteredStream returns EStreamEnumerationResult::Iterated). */
	bool HasRegisteredStreams() const;
	/** @return The streams registered with the server. */
	TArray<FReplicationStreamDescription> GetRegisteredStreams() const;
	
	/**
	 * Requests from the server to change the authority over some objects.
	 * @note The future may execute on any thread. Take care of synchronize correctly with the game thread if needed.
	 */
	virtual TFuture<UE::ConcertSyncClient::Replication::FAuthorityChangeResponse> RequestAuthorityChange(UE::ConcertSyncClient::Replication::FAuthorityChangeRequest Args) = 0;
	/** Util function that will request authority for all streams for the given objects. */
	TFuture<UE::ConcertSyncClient::Replication::FAuthorityChangeResponse> TakeAuthorityOver(TArrayView<const FSoftObjectPath> Objects);
	/** Util function that will let go over all authority of the given objects. */
	TFuture<UE::ConcertSyncClient::Replication::FAuthorityChangeResponse> ReleaseAuthorityOf(TArrayView<const FSoftObjectPath> Objects);

	/**
	 * Requests replication info about other clients, including the streams registered and which objects they have authority over (i.e. are sending).
	 * @note The future may execute on any thread. Take care to synchronize correctly with the game thread if needed.
	 */
	virtual TFuture<UE::ConcertSyncClient::Replication::FClientQueryResponse> QueryClientInfo(UE::ConcertSyncClient::Replication::FClientQueryRequest Args) = 0;

	/**
	 * Requests to change the client's registered streams
	 * @note The future may execute on any thread. Take care to synchronize correctly with the game thread if needed.
	 */
	virtual TFuture<UE::ConcertSyncClient::Replication::FChangeStreamResponse> ChangeStream(UE::ConcertSyncClient::Replication::FChangeStreamRequest Args) = 0;
	
	virtual ~IConcertClientReplicationManager() = default;
};
	