// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Replication/IConcertServerReplicationManager.h"
#include "Util/ClientServerCommunicationTest.h"
#include "Templates/UnrealTemplate.h"

class FAutomationTestBase;

namespace UE::ConcertSyncTests::Replication
{
	class FReplicationClient;

	/** Reusable logic to simulate a test server in spec tests with. */
	class FReplicationServer : public FNoncopyable
	{
	public:

		FReplicationServer(FAutomationTestBase& TestContext);

		/** Connects a client to the server. */
		FReplicationClient& ConnectClient();
		
		/** Lets the server process any messages that have come in. */
		void TickServer(float FakeDeltaTime = 1.f / 60.f);

		const TSharedRef<FConcertServerSessionMock>& GetServerSessionMock() const { return ServerSessionMock; }
	
	private:

		/** Used to test "obvious" cases that should never fail in any test. */
		FAutomationTestBase& TestContext;

		/** The underlying server session */
		TSharedRef<FConcertServerSessionMock> ServerSessionMock;
		/** Manages replication server side */
		TSharedRef<ConcertSyncServer::Replication::IConcertServerReplicationManager> ServerReplicationManager;

		/** Clients connected thus far. */
		TArray<TUniquePtr<FReplicationClient>> Clients;
	};
}


