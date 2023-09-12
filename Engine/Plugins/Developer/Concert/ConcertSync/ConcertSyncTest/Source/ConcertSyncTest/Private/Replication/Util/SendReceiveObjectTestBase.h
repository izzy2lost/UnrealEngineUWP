// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SendReceiveTestBase.h"

struct FConcertBatchReplicationEvent;
class IConcertClientReplicationManager;
class UTestReflectionObject;

namespace UE::ConcertSyncServer::Replication
{
	class IConcertServerReplicationManager;
}

namespace UE::ConcertSyncTests::Replication
{
	class FConcertClientReplicationBridgeMock;
	
	/**
	 * Creates a server and connects a sender and receiver client.
	 * The clients will be "sending" an UTestReflectionObject among each other.
	 */
	class FSendReceiveObjectTestBase : public FSendReceiveTestBase
	{
	public:

		FSendReceiveObjectTestBase(const FString& InName, const bool bInComplexTask)
			: FSendReceiveTestBase(InName, bInComplexTask)
		{}
		
		/**
		 * The object the test will be run on.
		 * 
		 * Note: this pointer is knowingly left dangling after the test completes...
		 * there really is not need to override CleanUpTest because we assign TestObject first thing in SetUpClientAndServer.
		 */
		UTestReflectionObject* TestObject = nullptr;

		//~ Begin FSendReceiveTestBase Interface
		virtual ConcertSyncClient::Replication::FJoinReplicatedSessionArgs CreateSenderArgs() override;
		virtual ConcertSyncClient::Replication::FJoinReplicatedSessionArgs CreateReceiverArgs() override;
		virtual void SetUpClientAndServer() override;
		//~ End FSendReceiveTestBase Interface
		
		/**
		 * bHasServerReceivedData and bHasClientReceivedData are reset to false prior to sending.
		 * Sets test values on TestObject and sends it to the receiver.
		 * If the data arrived, bHasServerReceivedData and bHasClientReceivedData are true.
		 */
		void SimulateSendObjectToReceiver(
			TFunctionRef<FReceiveReplicationEventSignature> OnServerReceive = [](auto&, auto&){},
			TFunctionRef<FReceiveReplicationEventSignature> OnReceiverClientReceive = [](auto&, auto&){}
			);

		void SetTestValues(UTestReflectionObject& Object);
		void SetDifferentValues(UTestReflectionObject& Object);
		void TestEqualTestValues(UTestReflectionObject& Object, FAutomationTestBase& Test);
		
	protected:
		
		const float SentFloat = 42.f;
		const FVector SentVector = { 21.f, 84.f, -1.f };
		
		const FGuid SenderStreamId = FGuid::NewGuid();

		virtual TSet<FGuid> GetSenderStreamIds() const { return { SenderStreamId }; } 
	};
}
