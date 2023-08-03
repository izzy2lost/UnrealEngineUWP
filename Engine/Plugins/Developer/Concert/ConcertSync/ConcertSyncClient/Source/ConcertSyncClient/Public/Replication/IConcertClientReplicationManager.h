// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Replication/Messages/ConcertReplicationHandshakeMessages.h"
#include "Replication/Data/ReplicationClientDescription.h"

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
}

/**
 * Handles all communication with the server regarding replication.
 * 
 * Keeps a list of properties to send along with their send rules.
 * Tells the server which properties this client is interested in receiving.
 */
class IConcertClientReplicationManager
{
public:

	/**
	 * Joins a replication session.
	 * Subsequent calls to JoinReplicationSession will fail until either the resulting TFuture returns or LeaveReplicationSession is called.
	 *
	 * @note This may execute on any thread. Take care to synchronize correctly with the game thread if needed.
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
		
	virtual ~IConcertClientReplicationManager() = default;
};