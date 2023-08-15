// Copyright Epic Games, Inc. All Rights Reserved.

#include "SendReceiveObjectTestBase.h"

#include "ConcertClientReplicationBridgeMock.h"
#include "Replication/Data/ReplicationStreamDescription.h"
#include "Replication/IConcertClientReplicationManager.h"
#include "Replication/IConcertServerReplicationManager.h"
#include "Replication/Messages/ConcertReplicationEvents.h"
#include "Replication/Messages/ConcertReplicationHandshakeMessages.h"
#include "Replication/PropertyChainUtils.h"
#include "Replication/ReplicationTestInterface.h"
#include "Replication/TestReflectionObject.h"
#include "Util/ClientServerCommunicationTest.h"

#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/Package.h"

namespace UE::ConcertSyncTests::Replication
{
	ConcertSyncClient::Replication::FJoinReplicatedSessionArgs FSendReceiveObjectTestBase::CreateSenderArgs()
	{
		ConcertSyncClient::Replication::FJoinReplicatedSessionArgs SenderJoinArgs;
		
		FReplicatedObjectInfo AllProperties { UTestReflectionObject::StaticClass() };
		ConcertSyncCore::PropertyChain::ForEachReplicatableConcertProperty(*UTestReflectionObject::StaticClass(), [&AllProperties](FConcertPropertyChain&& Chain)
		{
			AllProperties.PropertySelection.ReplicatedProperties.Emplace(MoveTemp(Chain));
			return EBreakBehavior::Continue;
		});

		FReplicationStreamDescription SendingStream;
		SendingStream.BaseDescription.Identifier = SenderStreamId;
		SendingStream.BaseDescription.ReplicationMap.ReplicatedObjects.Add(TestObject, AllProperties);
		SenderJoinArgs.Streams.Add(SendingStream);
		return SenderJoinArgs;
	}

	ConcertSyncClient::Replication::FJoinReplicatedSessionArgs FSendReceiveObjectTestBase::CreateReceiverArgs()
	{
		ConcertSyncClient::Replication::FJoinReplicatedSessionArgs ReceiverJoinArgs;
		// TODO DP: When we add client attributes, this test must be updated with attributes that allow receiving all data
		return ReceiverJoinArgs;
	}

	void FSendReceiveObjectTestBase::SetUpClientAndServer()
	{
		// Fake replicated object must be created before call to Super
		TestObject = NewObject<UTestReflectionObject>(GetTransientPackage());

		FSendReceiveTestBase::SetUpClientAndServer();
		
		// Bridge is responsible for telling client-side replication system about existing objects
		BridgeMock_Sender->InjectAvailableObject(*TestObject); 
		BridgeMock_Receiver->InjectAvailableObject(*TestObject);
	}
	
	void FSendReceiveObjectTestBase::SimulateSenderToReceiver(
		TFunctionRef<FReceiveReplicationEventSignature> OnServerReceive,
		TFunctionRef<FReceiveReplicationEventSignature> OnReceiverClientReceive)
	{
		auto TestReplicationData_Server = [this, OnServerReceive](const FConcertSessionContext& Context, const FConcertBatchReplicationEvent& Event)
		{
			TestEqual(TEXT("Server received 1 stream"), Event.Streams.Num(), 1);
			TestEqual(TEXT("Server received 1 object"), Event.Streams.IsEmpty() ? 0 : Event.Streams[0].ReplicatedObjects.Num() , 1);
			TestEqual(TEXT("Server received from correct stream"), Event.Streams.IsEmpty() ? FGuid{} : Event.Streams[0].StreamId, SenderStreamId);
			const FSoftObjectPath ObjectPath = Event.Streams.IsEmpty() || Event.Streams[0].ReplicatedObjects.IsEmpty() ? FSoftObjectPath{} : Event.Streams[0].ReplicatedObjects[0].ReplicatedObject;
			TestEqual(TEXT("Server's received object has correct path"), ObjectPath, FSoftObjectPath(TestObject));

			OnServerReceive(Context, Event);
		};
		auto TestReplicationData_Client_Receiver = [this, OnReceiverClientReceive](const FConcertSessionContext& Context, const FConcertBatchReplicationEvent& Event)
		{
			TestEqual(TEXT("Client 2 received 1 stream"), Event.Streams.Num(), 1);
			TestEqual(TEXT("Client 2 received 1 object"), Event.Streams.IsEmpty() ? 0 : Event.Streams[0].ReplicatedObjects.Num() , 1);
			TestEqual(TEXT("Client 2 received from correct stream"), Event.Streams.IsEmpty() ? FGuid{} : Event.Streams[0].StreamId, SenderStreamId);
			const FSoftObjectPath ObjectPath = Event.Streams.IsEmpty() || Event.Streams[0].ReplicatedObjects.IsEmpty() ? FSoftObjectPath{} : Event.Streams[0].ReplicatedObjects[0].ReplicatedObject;
			TestEqual(TEXT("Client 2's received object has correct path"), ObjectPath, FSoftObjectPath(TestObject));

			OnReceiverClientReceive(Context, Event);
		};
		const FDelegateHandle ServerHandle = ServerSession->RegisterCustomEventHandler<FConcertBatchReplicationEvent>(TestReplicationData_Server);
		const FDelegateHandle ClientHandle = Client_Receiver->ClientSessionMock->RegisterCustomEventHandler<FConcertBatchReplicationEvent>(TestReplicationData_Client_Receiver);

		
		// TestObject is the same UObject on both clients.
		// Hence we must override test values with SetTestValues and SetDifferentValues.
		// 1 Sender > Server
		SetTestValues(*TestObject);
		TickClient(Client_Sender);
		
		// 2 Forward from server to receiver
		TickServer();
		
		// 3 Receive from server
		SetDifferentValues(*TestObject);
		TickClient(Client_Receiver);

		ServerSession->UnregisterCustomEventHandler<FConcertBatchReplicationEvent>(ServerHandle);
		ServerSession->UnregisterCustomEventHandler<FConcertBatchReplicationEvent>(ClientHandle);
	}

	void FSendReceiveObjectTestBase::SetTestValues(UTestReflectionObject& Object)
	{
		Object.Float = SentFloat;
		Object.Vector = SentVector;
	}
	
	void FSendReceiveObjectTestBase::SetDifferentValues(UTestReflectionObject& Object)
	{
		Object.Float *= -1.f;
		Object.Vector *= -1.f;
	}
	
	void FSendReceiveObjectTestBase::TestEqualTestValues(UTestReflectionObject& Object, FAutomationTestBase& Test)
	{
		Test.TestEqual(TEXT("Float"), Object.Float, SentFloat);
		Test.TestEqual(TEXT("Vector"), Object.Vector, SentVector);
	}
}