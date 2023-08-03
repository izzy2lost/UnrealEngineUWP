// Copyright Epic Games, Inc. All Rights Reserved.

#include "Replication/Data/ReplicationStreamDescription.h"
#include "Replication/IConcertClientReplicationManager.h"
#include "Replication/IConcertServerReplicationManager.h"
#include "Replication/Messages/ConcertReplicationEvents.h"
#include "Replication/Messages/ConcertReplicationHandshakeMessages.h"
#include "Replication/PropertyChainUtils.h"
#include "Replication/ReplicationTestInterface.h"
#include "TestReflectionObject.h"
#include "Util/ConcertClientReplicationBridgeMock.h"
#include "Util/ConcertMocks.h"

#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/Package.h"

namespace UE::ConcertSyncTests
{
	static void SetTestValues(UTestReflectionObject& Object);
	static void SetDifferentValues(UTestReflectionObject& Object);
	static void TestEqualTestValues(UTestReflectionObject& Object, FAutomationTestBase& Test);

	static void TickClient(FConcertClientServerCommunicationTest::FClientInfo& Client);
	static void TickServer(FConcertServerSessionMock& ServerSession);

	/**
	 * Tests replicating data from sender client > server > receiver client.
	 */
	IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FSendReceiveLogicTests, FConcertClientServerCommunicationTest, "Concert.Replication.SendReceiveLogic", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);
	bool FSendReceiveLogicTests::RunTest(const FString& Parameters)
	{
		using namespace ConcertSyncClient::TestInterface;
		using namespace ConcertSyncServer::TestInterface;
		using namespace ConcertSyncClient::Replication;
		using namespace ConcertSyncServer::Replication;

		// 1. Init
		
		// Server
		InitServer();
		const TSharedPtr<FConcertServerSessionMock>& ServerSession = GetServerSessionMock();
		const TSharedRef<IConcertServerReplicationManager> ServerReplicationManager = CreateServerReplicationManager(ServerSession.ToSharedRef());
		
		// Client Receiver
		FClientInfo& Client_Receiver = ConnectClient();
		const TSharedRef<FConcertClientReplicationBridgeMock> BridgeMock_Receiver = MakeShared<FConcertClientReplicationBridgeMock>();
		const TSharedPtr<IConcertClientSession>& ClientSession_Receiver = Client_Receiver.ClientSessionMock;
		const TSharedRef<IConcertClientReplicationManager> ClientReplicationManager_Receiver = CreateClientReplicationManager(ClientSession_Receiver.ToSharedRef(), &BridgeMock_Receiver.Get());
		
		// Client Sender
		FClientInfo& Client_Sender = ConnectClient();
		const TSharedRef<FConcertClientReplicationBridgeMock> BridgeMock_Sender = MakeShared<FConcertClientReplicationBridgeMock>();
		const TSharedPtr<IConcertClientSession>& ClientSession_Sender = Client_Sender.ClientSessionMock;
		const TSharedRef<IConcertClientReplicationManager> ClientReplicationManager_Sender = CreateClientReplicationManager(ClientSession_Sender.ToSharedRef(), &BridgeMock_Sender.Get());

		// Fake replicated object
		UTestReflectionObject* TestObject = NewObject<UTestReflectionObject>(GetTransientPackage());
		const FSoftObjectPath TestObjectPath = TestObject;
		
		// 1.1 Sender offers to send all UTestReflectionObject properties
		const FGuid SenderStreamId = FGuid::NewGuid();
		{
			FReplicatedObjectInfo AllProperties { UTestReflectionObject::StaticClass() };
			ConcertSyncCore::PropertyChain::ForEachReplicatableConcertProperty(*UTestReflectionObject::StaticClass(), [&AllProperties](FConcertPropertyChain&& Chain)
			{
				AllProperties.PropertySelection.ReplicatedProperties.Emplace(MoveTemp(Chain));
				return EBreakBehavior::Continue;
			});
			
			FJoinReplicatedSessionArgs SenderJoinArgs;
			FReplicationStreamDescription SendingStream;
			SendingStream.BaseDescription.Identifier = SenderStreamId;
			SendingStream.BaseDescription.ReplicationMap.ReplicatedObjects.Add(TestObjectPath, AllProperties);
			SenderJoinArgs.Streams.Add(SendingStream);
			ClientReplicationManager_Sender->JoinReplicationSession(SenderJoinArgs)
				.Next([this](const FJoinReplicatedSessionResult& Result){ TestTrue(TEXT("Sender joined"), Result.ErrorCode == EJoinReplicationErrorCode::Success); });
		}

		// 1.2 Receiver accepts all properties
		{
			FJoinReplicatedSessionArgs ReceiverJoinArgs;
			// TODO: When we add client attributes, this test must be updated with attributes that allow receiving all data
			ClientReplicationManager_Receiver->JoinReplicationSession(ReceiverJoinArgs)
				.Next([this](const FJoinReplicatedSessionResult& Result){ TestTrue(TEXT("Receiver joined"), Result.ErrorCode == EJoinReplicationErrorCode::Success); });
		}
		
		// 1.3 Prepare test data
		bool bHasServerReceivedData = false;
		bool bHasClientReceivedData = false;
		auto TestReplicationData_Server = [&](const FConcertSessionContext& Context, const FConcertBatchReplicationEvent& Event)
		{
			if (bHasServerReceivedData)
			{
				AddError(TEXT("Server was expected to receive data exactly once!"));
				return;
			}
			bHasServerReceivedData = true;
			
			TestEqual(TEXT("Server received 1 stream"), Event.Streams.Num(), 1);
			TestEqual(TEXT("Server received 1 object"), Event.Streams.IsEmpty() ? 0 : Event.Streams[0].ReplicatedObjects.Num() , 1);
			TestEqual(TEXT("Server received from correct stream"), Event.Streams.IsEmpty() ? FGuid{} : Event.Streams[0].StreamId, SenderStreamId);
			const FSoftObjectPath ObjectPath = Event.Streams.IsEmpty() || Event.Streams[0].ReplicatedObjects.IsEmpty() ? FSoftObjectPath{} : Event.Streams[0].ReplicatedObjects[0].ReplicatedObject;
			TestEqual(TEXT("Server's received object has correct path"), ObjectPath, TestObjectPath);
		};
		auto TestReplicationData_Client_Sender = [&](const FConcertSessionContext& Context, const FConcertBatchReplicationEvent& Event)
        {
        	// There was a bug where client would receive its own data...
			AddError(TEXT("Client 1 was not supposed to receive data!"));
        };
		auto TestReplicationData_Client_Receiver = [&](const FConcertSessionContext& Context, const FConcertBatchReplicationEvent& Event)
		{
			if (bHasClientReceivedData)
			{
				AddError(TEXT("Client 2 was expected to receive data exactly once!"));
				return;
			}
			bHasClientReceivedData = true;
			
			TestEqual(TEXT("Client 2 received 1 stream"), Event.Streams.Num(), 1);
			TestEqual(TEXT("Client 2 received 1 object"), Event.Streams.IsEmpty() ? 0 : Event.Streams[0].ReplicatedObjects.Num() , 1);
			TestEqual(TEXT("Client 2 received from correct stream"), Event.Streams.IsEmpty() ? FGuid{} : Event.Streams[0].StreamId, SenderStreamId);
			const FSoftObjectPath ObjectPath = Event.Streams.IsEmpty() || Event.Streams[0].ReplicatedObjects.IsEmpty() ? FSoftObjectPath{} : Event.Streams[0].ReplicatedObjects[0].ReplicatedObject;
			TestEqual(TEXT("Client 2's received object has correct path"), ObjectPath, TestObjectPath);
		};
		ServerSession->RegisterCustomEventHandler<FConcertBatchReplicationEvent>(TestReplicationData_Server);
		Client_Sender.ClientSessionMock->RegisterCustomEventHandler<FConcertBatchReplicationEvent>(TestReplicationData_Client_Sender);
		Client_Receiver.ClientSessionMock->RegisterCustomEventHandler<FConcertBatchReplicationEvent>(TestReplicationData_Client_Receiver);

		// 1.4 Replication bridge
		// Bridge is responsible for telling client-side replication system about existing objects
		BridgeMock_Sender->InjectAvailableObject(*TestObject); 
		BridgeMock_Receiver->InjectAvailableObject(*TestObject);

		

		
		// 2. Send data
		// Note that the systems operate on the same UObject instance. This is required since the systems internally use object path.
		// For this reason we change the object after sending.
		
		// 2.1 Send from sender to server
		SetTestValues(*TestObject);
		TickClient(Client_Sender);
		
		// 2.2 Forward from server to receiver
		TickServer(*ServerSession);
		
		// 2.3 Receive from server
		SetDifferentValues(*TestObject);
		TickClient(Client_Receiver);




		
		// 3. Test
		// ... that the data went through server and receiving client ...
		TestTrue(TEXT("Server received replication event"), bHasServerReceivedData);
		TestTrue(TEXT("Client 2 received replication event"), bHasClientReceivedData);
		// ... and the data was applied correctly to the object
		TestEqualTestValues(*TestObject, *this);
		return true;
	}

	// TODO: Add more properties here
	constexpr float SentFloat = 42.f;
	const FVector SentVector = { 21.f, 84.f, -1.f };

	static void SetTestValues(UTestReflectionObject& Object)
	{
		Object.Float = SentFloat;
		Object.Vector = SentVector;
	}
	
	static void SetDifferentValues(UTestReflectionObject& Object)
	{
		Object.Float *= -1.f;
		Object.Vector *= -1.f;
	}
	
	static void TestEqualTestValues(UTestReflectionObject& Object, FAutomationTestBase& Test)
	{
		Test.TestEqual(TEXT("Float"), Object.Float, SentFloat);
		Test.TestEqual(TEXT("Vector"), Object.Vector, SentVector);
	}
	
	constexpr float FakeDeltaTime = 1.f / 60.f;
	
	static void TickClient(FConcertClientServerCommunicationTest::FClientInfo& Client)
	{
		Client.ClientSessionMock->OnTick().Broadcast(*Client.ClientSessionMock, FakeDeltaTime);
	}

	static void TickServer(FConcertServerSessionMock& ServerSession)
	{
		ServerSession.OnTick().Broadcast(ServerSession, FakeDeltaTime);
	}
}
