// Copyright Epic Games, Inc. All Rights Reserved.

#include "Util/SendReceiveObjectTestBase.h"

#include "Replication/IConcertClientReplicationManager.h"
#include "TestReflectionObject.h"

#include "Components/StaticMeshComponent.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Util/SendReceiveGenericStreamTestBase.h"

namespace UE::ConcertSyncTests::Replication::Authority
{
	/** Data sent from a client that does not have authority over objects is rejected. */
	IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FRejectUnauthorativeClientTest, FSendReceiveObjectTestBase, "Concert.Replication.Authority.RejectUnauthorativeClient", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);
	bool FRejectUnauthorativeClientTest::RunTest(const FString& Parameters)
	{
		// 1. Init
		SetUpClientAndServer();
		
		// 2. Send data without authority > Reject
		bool bHasServerReceivedData = false;
		auto OnServerReceive = [this, &bHasServerReceivedData](const FConcertSessionContext& Context, const FConcertBatchReplicationEvent& Event) mutable
		{
			if (bHasServerReceivedData)
			{
				AddError(TEXT("Server was expected to receive data exactly once!"));
			}
			bHasServerReceivedData = true;
		};
		auto OnClientReceive = [this](const FConcertSessionContext& Context, const FConcertBatchReplicationEvent& Event) mutable
		{
			AddError(TEXT("Server sent data from non-authorative client to receiving client!"));
		};
		SimulateSenderToReceiver(OnServerReceive, OnClientReceive);
		TestTrue(TEXT("Server received replication event from non-authorative client"), bHasServerReceivedData);
		
		return true;
	}

	/** Makes sure only one client can take authority over the same object properties at the same time and that other clients continue being able to take authority after authority has been released.  */
	IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FAuthorityIsMutallyExclusiveTest, FSendReceiveGenericTestBase, "Concert.Replication.Authority.AuthorityIsMutallyExclusive", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);
	bool FAuthorityIsMutallyExclusiveTest::RunTest(const FString& Parameters)
	{
		// 1. Init
		const UObject* UnusedTestObject = GetDefault<UTestReflectionObject>();
		const FSoftObjectPath TestObjectPath { UnusedTestObject };
		SenderArgs = CreateHandshakeArgsFrom(*UnusedTestObject);
		ReceiverArgs = SenderArgs;
		
		SetUpClientAndServer();

		// 2.1 Senders takes authority over an object...
		bool bSenderReceivedResponse = false;
		ClientReplicationManager_Sender->TakeAuthorityOver({ TestObjectPath })
			.Next([this, &bSenderReceivedResponse](const ConcertSyncClient::Replication::FAuthorityChangeResponse& Response) mutable
			{
				bSenderReceivedResponse = true;
				TestEqual(TEXT("No rejection taking authority"), Response.RejectedObjects.Num(), 0); 
			});
		TickClient(Client_Sender);
		TickServer();
		TestTrue(TEXT("Sender received response to take authority"), bSenderReceivedResponse);

		// 2.2 ... so does receiver fails taking authority over the same object
		bool bReceiverReceivedResponse = false;
		ClientReplicationManager_Receiver->TakeAuthorityOver({ TestObjectPath })
			.Next([this, &bReceiverReceivedResponse](const ConcertSyncClient::Replication::FAuthorityChangeResponse& Response) mutable
			{
				bReceiverReceivedResponse = true;
				TestEqual(TEXT("Rejected because Sender already has authority"), Response.RejectedObjects.Num(), 1); 
			});
		TickClient(Client_Receiver);
		TickServer();
		TestTrue(TEXT("Receiver received rejection response"), bReceiverReceivedResponse);

		// 2.3 ... then the sender lets go of authority ...
		bSenderReceivedResponse = false;
		ClientReplicationManager_Sender->ReleaseAuthorityOf({ TestObjectPath })
			.Next([this, &bSenderReceivedResponse](const ConcertSyncClient::Replication::FAuthorityChangeResponse& Response) mutable
			{
				bSenderReceivedResponse = true;
				TestEqual(TEXT("No rejection releasing object"), Response.RejectedObjects.Num(), 0); 
			});
		TickClient(Client_Sender);
		TickServer();
		TestTrue(TEXT("Sender received response releasing object"), bSenderReceivedResponse);

		// 2.4 ... and the receiver can take authority of the object
		bReceiverReceivedResponse = false;
		ClientReplicationManager_Receiver->TakeAuthorityOver({ TestObjectPath })
			.Next([this, &bReceiverReceivedResponse](const ConcertSyncClient::Replication::FAuthorityChangeResponse& Response) mutable
			{
				bReceiverReceivedResponse = true;
				TestEqual(TEXT("No rejection because the object should not be released"), Response.RejectedObjects.Num(), 0); 
			});
		TickClient(Client_Receiver);
		TickServer();
		TestTrue(TEXT("Receiver received response taking authority"), bReceiverReceivedResponse);
		
		return true;
	}
	
	/** Clients cannot take authority over objects that they did not send */
	IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FCannotTakeAuthorityOverUnregisteredObjectsTest, FSendReceiveGenericTestBase, "Concert.Replication.Authority.CannotTakeAuthorityOverUnregisteredObjects", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);
	bool FCannotTakeAuthorityOverUnregisteredObjectsTest::RunTest(const FString& Parameters)
	{
		// FSendReceiveGenericTestBase is used here because it registers no sending streams by default
		
		// 1. Init
		SetUpClientAndServer();

		// 2. Attempt to take authority over object that was not in handshake
		bool bReceivedResponse = false;
		const FSoftObjectPath SomePath(GetDefault<UTestReflectionObject>());
		ConcertSyncClient::Replication::FAuthorityChangeRequest Request;
		Request.TakeAuthority.Add(SomePath, FConcertStreamArray{ .StreamIds = { FGuid::NewGuid() } });
		// Detail: Cannot use IConcertClientReplicationManager::TakeAuthority util here because it builds the request based on what streams were registered
		ClientReplicationManager_Sender->RequestAuthorityChange(Request)
			.Next([this, &bReceivedResponse, SomePath](const ConcertSyncClient::Replication::FAuthorityChangeResponse& Response) mutable
			{
				bReceivedResponse = true;
				TestTrue(TEXT("Cannot take authority over unregistered object"), Response.RejectedObjects.Contains(SomePath));
				TestEqual(TEXT("Rejected exactly 1 object"), Response.RejectedObjects.Num(), 1); 
			});

		// 3. Authority request was rejected
		SimulateSenderToReceiver();
		TestTrue(TEXT("Authority request was answered"), bReceivedResponse);
		
		return true;
	}

	/** When a client leaves the replication session, the client releases all of the authority as well.  */
	IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FClientLeavingReplicationSessionLosesAuthority, FSendReceiveGenericTestBase, "Concert.Replication.Authority.ClientLeavingReplicationSessionLosesAuthority", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);
	bool FClientLeavingReplicationSessionLosesAuthority::RunTest(const FString& Parameters)
	{
		// 1. Init
		const UObject* UnusedTestObject = GetDefault<UTestReflectionObject>();
		const FSoftObjectPath TestObjectPath { UnusedTestObject };
		SenderArgs = CreateHandshakeArgsFrom(*UnusedTestObject);
		ReceiverArgs = SenderArgs;
		
		SetUpClientAndServer();

		// 2.1 Senders takes authority over an object...
		bool bSenderReceivedResponse = false;
		ClientReplicationManager_Sender->TakeAuthorityOver({ TestObjectPath })
			.Next([this, &bSenderReceivedResponse](const ConcertSyncClient::Replication::FAuthorityChangeResponse& Response) mutable
			{
				bSenderReceivedResponse = true;
				TestEqual(TEXT("No rejection taking authority (sender)"), Response.RejectedObjects.Num(), 0); 
			});
		TickClient(Client_Sender);
		TickServer();
		TestTrue(TEXT("Sender received response to take authority"), bSenderReceivedResponse);

		// 2.2 ... then leaves
		ClientReplicationManager_Sender->LeaveReplicationSession();
		TickClient(Client_Sender);
		TickServer();
		
		// 2.3 which means the receiver is allowed to take authority
		bool bReceiverReceivedResponse = false;
		ClientReplicationManager_Receiver->TakeAuthorityOver({ TestObjectPath })
			.Next([this, &bReceiverReceivedResponse](const ConcertSyncClient::Replication::FAuthorityChangeResponse& Response) mutable
			{
				bReceiverReceivedResponse = true;
				TestEqual(TEXT("No rejection taking authority (receiver)"), Response.RejectedObjects.Num(), 0); 
			});
		TickClient(Client_Receiver);
		TickServer();
		TestTrue(TEXT("Receiver received response to take authority"), bReceiverReceivedResponse);
		
		return true;
	}

	/** When a client leaves the replication session, the client releases all of the authority as well.  */
	IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FClientLeavingConcertSessionLosesAuthority, FSendReceiveGenericTestBase, "Concert.Replication.Authority.ClientLeavingConcertSessionLosesAuthority", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);
	bool FClientLeavingConcertSessionLosesAuthority::RunTest(const FString& Parameters)
	{
		// 1. Init
		const UObject* UnusedTestObject = GetDefault<UTestReflectionObject>();
		const FSoftObjectPath TestObjectPath { UnusedTestObject };
		SenderArgs = CreateHandshakeArgsFrom(*UnusedTestObject);
		ReceiverArgs = SenderArgs;
		
		SetUpClientAndServer();

		// 2.1 Senders takes authority over an object...
		bool bSenderReceivedResponse = false;
		ClientReplicationManager_Sender->TakeAuthorityOver({ TestObjectPath })
			.Next([this, &bSenderReceivedResponse](const ConcertSyncClient::Replication::FAuthorityChangeResponse& Response) mutable
			{
				bSenderReceivedResponse = true;
				TestEqual(TEXT("No rejection taking authority (sender)"), Response.RejectedObjects.Num(), 0); 
			});
		TickClient(Client_Sender);
		TickServer();
		TestTrue(TEXT("Sender received response to take authority"), bSenderReceivedResponse);

		// 2.2 ... then leaves
		Client_Sender->ClientSessionMock->Disconnect();
		TickClient(Client_Sender);
		TickServer();
		
		// 2.3 which means the receiver is allowed to take authority
		bool bReceiverReceivedResponse = false;
		ClientReplicationManager_Receiver->TakeAuthorityOver({ TestObjectPath })
			.Next([this, &bReceiverReceivedResponse](const ConcertSyncClient::Replication::FAuthorityChangeResponse& Response) mutable
			{
				bReceiverReceivedResponse = true;
				TestEqual(TEXT("No rejection taking authority (receiver)"), Response.RejectedObjects.Num(), 0); 
			});
		TickClient(Client_Receiver);
		TickServer();
		TestTrue(TEXT("Receiver received response to take authority"), bReceiverReceivedResponse);
		
		return true;
	}
}
