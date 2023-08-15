// Copyright Epic Games, Inc. All Rights Reserved.

#include "Util/SendReceiveObjectTestBase.h"

#include "Replication/Data/ReplicationStreamDescription.h"
#include "Replication/IConcertClientReplicationManager.h"
#include "Replication/IConcertServerReplicationManager.h"
#include "Replication/Messages/ConcertReplicationEvents.h"
#include "Replication/Messages/ConcertReplicationHandshakeMessages.h"
#include "Replication/PropertyChainUtils.h"
#include "Replication/ReplicationTestInterface.h"
#include "TestReflectionObject.h"
#include "Util/ConcertClientReplicationBridgeMock.h"
#include "Util/ClientServerCommunicationTest.h"

#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/Package.h"

namespace UE::ConcertSyncTests::Replication
{
	/**
	 * Tests replicating data from sender client > server > receiver client.
	 */
	IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FSendReceiveFlowTests, FSendReceiveObjectTestBase, "Concert.Replication.SendReceiveFlow", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);
	bool FSendReceiveFlowTests::RunTest(const FString& Parameters)
	{
		// 1. Init
		SetUpClientAndServer();
		
		// 2. Send data
		bool bHasServerReceivedData = false;
		bool bHasClientReceivedData = false;
		auto OnServerReceive = [this, &bHasServerReceivedData](const FConcertSessionContext& Context, const FConcertBatchReplicationEvent& Event) mutable
		{
			if (bHasServerReceivedData)
			{
				AddError(TEXT("Server was expected to receive data exactly once!"));
			}
			bHasServerReceivedData = true;
		};
		auto OnClientReceive = [this, &bHasClientReceivedData](const FConcertSessionContext& Context, const FConcertBatchReplicationEvent& Event) mutable
		{
			if (bHasClientReceivedData)
			{
				AddError(TEXT("Client 2 was expected to receive data exactly once!"));
			}
			bHasClientReceivedData = true;
		};
		SimulateSenderToReceiver(OnServerReceive, OnClientReceive);

		// 3. Test
		TestTrue(TEXT("Server received replication event"), bHasServerReceivedData);
		TestTrue(TEXT("Client 2 received replication event"), bHasClientReceivedData);
		TestEqualTestValues(*TestObject, *this);
		
		return true;
	}
}
