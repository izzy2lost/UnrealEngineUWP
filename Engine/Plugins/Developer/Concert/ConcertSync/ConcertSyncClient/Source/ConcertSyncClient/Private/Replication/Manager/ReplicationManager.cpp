// Copyright Epic Games, Inc. All Rights Reserved.

#include "ReplicationManager.h"

#include "ConcertLogGlobal.h"
#include "ConcertSyncClientLiveSession.h"
#include "IConcertSession.h"
#include "ReplicationManagerState_Disconnected.h"

#include "Replication/Messages/ConcertReplicationHandshakeMessages.h"

namespace UE::ConcertSyncClient::Replication
{
	FReplicationManager::FReplicationManager(TSharedRef<IConcertClientSession> InLiveSession, IConcertClientReplicationBridge* InBridge)
		: Session(MoveTemp(InLiveSession))
		, Bridge(InBridge)
	{}

	FReplicationManager::~FReplicationManager()
	{}

	void FReplicationManager::StartAcceptingJoinRequests()
	{
		checkSlow(!CurrentState.IsValid());
		CurrentState = MakeShared<FReplicationManagerState_Disconnected>(Session, Bridge, *this);
	}

	TFuture<FJoinReplicatedSessionResult> FReplicationManager::JoinReplicationSession(FJoinReplicatedSessionArgs Args)
	{
		return ensureMsgf(CurrentState, TEXT("StartAcceptingJoinRequests should have been called at this point."))
			? CurrentState->JoinReplicationSession(MoveTemp(Args))
			: MakeFulfilledPromise<FJoinReplicatedSessionResult>(EJoinReplicationErrorCode::Cancelled).GetFuture();
	}

	void FReplicationManager::LeaveReplicationSession()
	{
		if (ensureMsgf(CurrentState, TEXT("StartAcceptingJoinRequests should have been called at this point.")))
		{
			CurrentState->LeaveReplicationSession();
		}
	}

	bool FReplicationManager::CanJoin()
	{
		return ensureMsgf(CurrentState, TEXT("StartAcceptingJoinRequests should have been called at this point."))
			&& CurrentState->CanJoin(); 
	}

	bool FReplicationManager::IsConnectedToReplicationSession()
	{
		return ensureMsgf(CurrentState, TEXT("StartAcceptingJoinRequests should have been called at this point."))
			&& CurrentState->IsConnectedToReplicationSession(); 
	}

	void FReplicationManager::OnChangeState(TSharedRef<FReplicationManagerState> NewState)
	{
		CurrentState = MoveTemp(NewState);
	}
}

