// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Replication/IConcertClientReplicationManager.h"
#include "Replication/Messages/ConcertReplicationHandshakeMessages.h"
#include "Templates/SharedPointer.h"

class IConcertClientReplicationBridge;
class IConcertClientSession;

namespace UE::ConcertSyncClient::Replication
{
	class FReplicationManagerState;
	
	class FReplicationManager : public IConcertClientReplicationManager
	{
		friend FReplicationManagerState; 
	public:
		
		FReplicationManager(TSharedRef<IConcertClientSession> InLiveSession, IConcertClientReplicationBridge* InBridge);
		virtual ~FReplicationManager() override;

		/** Starts accepting join requests. Must be called separately from constructor because of TSharedFromThis asserting if SharedThis is called in constructor. */
		void StartAcceptingJoinRequests();

		//~ Begin IConcertClientReplicationManager Interface
		virtual TFuture<FJoinReplicatedSessionResult> JoinReplicationSession(FJoinReplicatedSessionArgs Args) override;
		virtual void LeaveReplicationSession() override;
		virtual bool CanJoin() override;
		virtual bool IsConnectedToReplicationSession() override;
		//~ End IConcertClientReplicationManager Interface

	private:
		
		/** Session instance this manager was created for. */
		TSharedRef<IConcertClientSession> Session;
		/** The replication bridge is responsible for applying received data and generating data to send. */
		IConcertClientReplicationBridge* Bridge;

		/** The current state this manager is in, e.g. waiting for connection request, connecting, connected, etc. */
		TSharedPtr<FReplicationManagerState> CurrentState;
		
		/** Called by FReplicationManagerState to change the state. */
		void OnChangeState(TSharedRef<FReplicationManagerState> NewState);
	};
}
