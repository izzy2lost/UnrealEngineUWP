// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Replication/IConcertClientReplicationManager.h"
#include "Util/ClientServerCommunicationTest.h"

class IConcertClientReplicationManager;
class UTestReflectionObject;

namespace UE::ConcertSyncClient::Replication
{
	struct FJoinReplicatedSessionArgs;
}
namespace UE::ConcertSyncServer::Replication
{
	class IConcertServerReplicationManager;
}

namespace UE::ConcertSyncTests::Replication
{
	class FConcertClientReplicationBridgeMock;
	
	/** Creates a server, connects a sender and receiver client, and completes a handshake for them. */
	class FSendReceiveTestBase : public FConcertClientServerCommunicationTest
	{
	public:
	
		FSendReceiveTestBase(const FString& InName, const bool bInComplexTask);

	protected:
		
		float FakeDeltaTime = 1.f / 60.f;
		
		TSharedPtr<FConcertServerSessionMock> ServerSession;
		TSharedPtr<ConcertSyncServer::Replication::IConcertServerReplicationManager> ServerReplicationManager;
		
		FClientInfo* Client_Receiver = nullptr;
		TSharedPtr<FConcertClientReplicationBridgeMock> BridgeMock_Receiver;
		TSharedPtr<IConcertClientReplicationManager> ClientReplicationManager_Receiver;

		FClientInfo* Client_Sender  = nullptr;
		TSharedPtr<FConcertClientReplicationBridgeMock> BridgeMock_Sender;
		TSharedPtr<IConcertClientReplicationManager> ClientReplicationManager_Sender;

		virtual ConcertSyncClient::Replication::FJoinReplicatedSessionArgs CreateSenderArgs() = 0;
		virtual ConcertSyncClient::Replication::FJoinReplicatedSessionArgs CreateReceiverArgs() = 0;

		virtual void SetUpClientAndServer();
		void SimulateSenderToReceiver();
		
		void TickClient(FClientInfo* Client);
		void TickServer();
		
	private:
		
		virtual void CleanUpTest(FAutomationTestBase* AutomationTestBase) override;
	};
}
