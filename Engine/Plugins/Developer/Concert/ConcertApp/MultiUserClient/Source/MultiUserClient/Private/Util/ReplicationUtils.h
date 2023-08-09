// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Async/Future.h"

namespace UE::ConcertSyncClient::Replication
{
	struct FJoinReplicatedSessionResult;
	struct FJoinReplicatedSessionArgs;
}

class IConcertClientReplicationManager;

namespace UE::MultiUserClient::ReplicationUtils
{
	/**
	 * Requests to join the replication session in the currently.
	 * 
	 * This decorates the request with fancy editor UI notifications on the bottom-right: One for the duration of the process,
	 * and then another for either success (leaves after a few seconds) or failure (must be dismissed).
	 */
	TFuture<ConcertSyncClient::Replication::FJoinReplicatedSessionResult> JoinSessionWithEditorNotifications(
		IConcertClientReplicationManager& Manager,
		ConcertSyncClient::Replication::FJoinReplicatedSessionArgs Args
		);

	/** Leaves the replications session and shows an editor UI notification on the bottom-right. */
	void LeaveSessionWithEditorNotifications(IConcertClientReplicationManager& Manager);
}
