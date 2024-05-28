// Copyright Epic Games, Inc. All Rights Reserved.

#include "ObjectTestReplicator.h"

#include "ReplicationClient.h"
#include "ReplicationServer.h"
#include "Replication/PropertyChainUtils.h"
#include "Replication/Messages/ObjectReplication.h"

namespace UE::ConcertSyncTests::Replication
{
	TSharedRef<FObjectTestReplicator> FObjectTestReplicator::CreateSubobjectReplicator() const
	{
		UTestReflectionObject* Subobject = NewObject<UTestReflectionObject>(TestObject);
		TestObject->InstancedSubobject = Subobject;
		return MakeShared<FObjectTestReplicator>(Subobject);
	}

	ConcertSyncClient::Replication::FJoinReplicatedSessionArgs FObjectTestReplicator::CreateSenderArgs(
		FGuid SenderStreamId,
		EConcertObjectReplicationMode ReplicationMode,
		uint8 ReplicationRate
		) const
	{
		ConcertSyncClient::Replication::FJoinReplicatedSessionArgs SenderJoinArgs;
		SenderJoinArgs.Streams.Add(CreateStream(SenderStreamId, ReplicationMode, ReplicationRate));
		return SenderJoinArgs;
	}

	FConcertReplicationStream FObjectTestReplicator::CreateStream(FGuid SenderStreamId, EConcertObjectReplicationMode ReplicationMode, uint8 ReplicationRate) const
	{
		FConcertReplicatedObjectInfo ReplicatedObjectInfo { TestObject->GetClass() };
		ConcertSyncCore::PropertyChain::ForEachReplicatableConcertProperty(*TestObject->GetClass(), [&ReplicatedObjectInfo](FConcertPropertyChain&& Chain)
		{
			ReplicatedObjectInfo.PropertySelection.ReplicatedProperties.Emplace(MoveTemp(Chain));
			return EBreakBehavior::Continue;
		});

		FConcertReplicationStream SendingStream;
		SendingStream.BaseDescription.Identifier = SenderStreamId;
		SendingStream.BaseDescription.ReplicationMap.ReplicatedObjects.Add(TestObject, ReplicatedObjectInfo);
		SendingStream.BaseDescription.FrequencySettings.Defaults = { ReplicationMode, ReplicationRate };
		return SendingStream;
	}

	void FObjectTestReplicator::SimulateSendObjectToReceiver(
		FAutomationTestBase& Test,
		FObjectReplicationContext Context,
		TConstArrayView<FGuid> SenderStreams,
		TFunctionRef<FReceiveReplicationEventSignature> OnServerReceive,
		TFunctionRef<FReceiveReplicationEventSignature> OnReceiverClientReceive,
		EPropertyReplicationFlags PropertyFlags
		) const
	{
		auto TestReplicationData_Server = [this, &Test, &SenderStreams, &OnServerReceive](const FConcertSessionContext& Context, const FConcertReplication_BatchReplicationEvent& Event)
		{
			Test.TestEqual(TEXT("Server received right number of streams"), Event.Streams.Num(), SenderStreams.Num());
			for (int32 i = 0; i < Event.Streams.Num(); ++i)
			{
				Test.TestEqual(TEXT("Server received 1 object"), Event.Streams[i].ReplicatedObjects.Num() , 1);
				Test.TestTrue(TEXT("Server received from correct stream"), SenderStreams.Contains(Event.Streams[i].StreamId));
				const FSoftObjectPath ObjectPath = Event.Streams[i].ReplicatedObjects.IsEmpty() ? FSoftObjectPath{} : Event.Streams[i].ReplicatedObjects[0].ReplicatedObject;
				Test.TestEqual(TEXT("Server's received object has correct path"), ObjectPath, FSoftObjectPath(TestObject));
			}
			OnServerReceive(Context, Event);
		};
		auto TestReplicationData_Client_Receiver = [this, &Test, &SenderStreams, &OnReceiverClientReceive](const FConcertSessionContext& Context, const FConcertReplication_BatchReplicationEvent& Event)
		{
			Test.TestEqual(TEXT("Client 2 received right number of streams"), Event.Streams.Num(), SenderStreams.Num());
			for (int32 i = 0; i < Event.Streams.Num(); ++i)
			{
				Test.TestEqual(TEXT("Client 2 received 1 object"), Event.Streams[i].ReplicatedObjects.Num() , 1);
				Test.TestTrue(TEXT("Client 2 received from correct stream"), SenderStreams.Contains(Event.Streams[i].StreamId));
				const FSoftObjectPath ObjectPath = Event.Streams[i].ReplicatedObjects.IsEmpty() ? FSoftObjectPath{} : Event.Streams[i].ReplicatedObjects[0].ReplicatedObject;
				Test.TestEqual(TEXT("Client 2's received object has correct path"), ObjectPath, FSoftObjectPath(TestObject));
			}
			OnReceiverClientReceive(Context, Event);
		};
		const FDelegateHandle ServerHandle = Context.Server.GetServerSessionMock()->RegisterCustomEventHandler<FConcertReplication_BatchReplicationEvent>(TestReplicationData_Server);
		const FDelegateHandle ClientHandle = Context.Receiver.GetClientSessionMock()->RegisterCustomEventHandler<FConcertReplication_BatchReplicationEvent>(TestReplicationData_Client_Receiver);

		
		// TestObject is the same UObject on both clients.
		// Hence we must override test values with SetTestValues and SetDifferentValues.
		// 1 Sender > Server
		SetTestValues(PropertyFlags);
		Context.Sender.TickClient();
		
		// 2 Forward from server to receiver
		Context.Server.TickServer();
		
		// 3 Receive from server
		SetDifferentValues(PropertyFlags);
		Context.Receiver.TickClient();

		Context.Server.GetServerSessionMock()->UnregisterCustomEventHandler<FConcertReplication_BatchReplicationEvent>(ServerHandle);
		Context.Receiver.GetClientSessionMock()->UnregisterCustomEventHandler<FConcertReplication_BatchReplicationEvent>(ClientHandle);
	}

	void FObjectTestReplicator::SetTestValues(EPropertyReplicationFlags PropertyFlags) const
	{
		const bool bSendCDOValues = EnumHasAnyFlags(PropertyFlags, EPropertyReplicationFlags::SendCDOValues);
		if (EnumHasAnyFlags(PropertyFlags, EPropertyReplicationFlags::Float))
		{
			TestObject->Float = bSendCDOValues ? GetMutableDefault<UTestReflectionObject>()->Float : SentFloat;
		}
		if (EnumHasAnyFlags(PropertyFlags, EPropertyReplicationFlags::Vector))
		{
			TestObject->Vector = bSendCDOValues ? GetMutableDefault<UTestReflectionObject>()->Vector : SentVector;
		}
	}

	void FObjectTestReplicator::SetDifferentValues(EPropertyReplicationFlags PropertyFlags) const
	{
		if (EnumHasAnyFlags(PropertyFlags, EPropertyReplicationFlags::Float))
		{
			TestObject->Float = DifferentFloat;
		}
		if (EnumHasAnyFlags(PropertyFlags, EPropertyReplicationFlags::Vector))
		{
			TestObject->Vector = DifferentVector;
		}
	}

	void FObjectTestReplicator::TestValuesWereReplicated(FAutomationTestBase& Test, EPropertyReplicationFlags PropertyFlags) const
	{
		const bool bSendCDOValues = EnumHasAnyFlags(PropertyFlags, EPropertyReplicationFlags::SendCDOValues);
		if (EnumHasAnyFlags(PropertyFlags, EPropertyReplicationFlags::Float))
		{
			const float ExpectedValue = bSendCDOValues ? GetMutableDefault<UTestReflectionObject>()->Float : SentFloat;
			const bool bWasReplicated = Test.TestEqual(TEXT("Float"), TestObject->Float, ExpectedValue);
			Test.AddErrorIfFalse(bWasReplicated, TEXT("Failed to replicate \"Float\" property"));
		}
		if (EnumHasAnyFlags(PropertyFlags, EPropertyReplicationFlags::Vector))
		{
			const FVector ExpectedValue = bSendCDOValues ? GetMutableDefault<UTestReflectionObject>()->Vector : SentVector;
			const bool bWasReplicated = Test.TestEqual(TEXT("Vector"), TestObject->Vector, ExpectedValue);
			Test.AddErrorIfFalse(bWasReplicated, TEXT("Failed to replicate \"Vector\" property"));
		}
	}

	void FObjectTestReplicator::TestValuesWereNotReplicated(FAutomationTestBase& Test, EPropertyReplicationFlags PropertyFlags) const
	{
		if (EnumHasAnyFlags(PropertyFlags, EPropertyReplicationFlags::Float))
		{
			const bool bWasNotReplicated = Test.TestEqual(TEXT("Float"), TestObject->Float, DifferentFloat);
			Test.AddErrorIfFalse(bWasNotReplicated, TEXT("Probably \"Float\" property was replicated even though it was not supposed to be!"));
		}
		if (EnumHasAnyFlags(PropertyFlags, EPropertyReplicationFlags::Vector))
		{
			const bool bWasNotReplicated = Test.TestEqual(TEXT("Vector"), TestObject->Vector, DifferentVector);
			Test.AddErrorIfFalse(bWasNotReplicated, TEXT("Probably \"Vector\" property was replicated even though it was not supposed to be!"));
		}
	}
}
