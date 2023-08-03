// Copyright Epic Games, Inc. All Rights Reserved.

#include "ReplicationManagerState_Disconnected.h"

#include "ConcertLogGlobal.h"
#include "ReplicationManagerState_Handshaking.h"

namespace UE::ConcertSyncClient::Replication
{
	FReplicationManagerState_Disconnected::FReplicationManagerState_Disconnected(
		TSharedRef<IConcertClientSession> LiveSession,
		IConcertClientReplicationBridge* ReplicationBridge,
			FReplicationManager& Owner
		)
		: FReplicationManagerState(Owner)
		, LiveSession(MoveTemp(LiveSession))
		, ReplicationBridge(ReplicationBridge)
	{}

	TFuture<FJoinReplicatedSessionResult> FReplicationManagerState_Disconnected::JoinReplicationSession(FJoinReplicatedSessionArgs Args)
	{
		TPromise<FJoinReplicatedSessionResult> JoinPromise;
		TFuture<FJoinReplicatedSessionResult> JoinFuture = JoinPromise.GetFuture();
		ChangeState(MakeShared<FReplicationManagerState_Handshaking>(MoveTemp(Args), MoveTemp(JoinPromise), LiveSession, ReplicationBridge, GetOwner()));
		return JoinFuture;
	}

	void FReplicationManagerState_Disconnected::LeaveReplicationSession()
	{
		UE_LOG(LogConcert, Warning, TEXT("LeaveReplicationSession: Already disconnected."));
	}
}
