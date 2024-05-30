// Copyright Epic Games, Inc. All Rights Reserved.

#include "ReplicationServer.h"

#include "ReplicationClient.h"
#include "Replication/ReplicationTestInterface.h"

namespace UE::ConcertSyncTests::Replication
{
	FReplicationServer::FReplicationServer(FAutomationTestBase& TestContext, EConcertSyncSessionFlags InSessionFlags)
		: SessionFlags(InSessionFlags)
		, TestContext(TestContext)
		, ServerSessionMock(MakeShared<FConcertServerSessionMock>())
		, ServerReplicationManager(ConcertSyncServer::TestInterface::CreateServerReplicationManager(ServerSessionMock, InSessionFlags))
	{}

	FReplicationClient& FReplicationServer::ConnectClient()
	{
		const FGuid ClientEndpointId(0, 0, 0, Clients.Num() + 1); // {0, 0, 0, 0} is used by the server.
		Clients.Add(MakeUnique<FReplicationClient>(ClientEndpointId, SessionFlags, *ServerSessionMock, TestContext));
		ServerSessionMock->ConnectClient(ClientEndpointId, *(Clients.Last()->GetClientSessionMock()));
		return *Clients.Last();
	}

	void FReplicationServer::TickServer(float FakeDeltaTime)
	{
		ServerSessionMock->OnTick().Broadcast(*ServerSessionMock, FakeDeltaTime);
	}
}
