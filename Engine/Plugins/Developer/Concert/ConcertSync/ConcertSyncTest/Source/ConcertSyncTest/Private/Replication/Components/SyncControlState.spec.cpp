// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#include "Replication/SyncControlState.h"
#include "Replication/Messages/SyncControl.h"
#include "Replication/Util/Spec/ObjectTestReplicator.h"

namespace UE::ConcertSyncTests::Replication::UI
{
	BEGIN_DEFINE_SPEC(FSyncControlStateSpec, "Editor.Concert.Replication.Components", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
	FGuid SenderStreamId = FGuid::NewGuid();
	/** Leverage to create a real FSoftObjectPath. */
	TSharedPtr<FObjectTestReplicator> ObjectReplicator;
	END_DEFINE_SPEC(FSyncControlStateSpec);

	/** This tests that FSyncControlState correctly analyses requests and responses for aggregation. */
	void FSyncControlStateSpec::Define()
	{
		BeforeEach([this]
		{
			ObjectReplicator = MakeShared<FObjectTestReplicator>();
		});
		AfterEach([this]
		{
			// Test would hold onto this for rest of engine lifetime. Clean up this mini would-be leak.
			ObjectReplicator.Reset();
		});

		// At this point, everything should work server-side. Now test client-side prediction.
		Describe("Changing sync control state", [this]()
		{
			It("Is correct with releasing authority with FConcertReplication_ChangeAuthority_Request", [this]()
			{
				ConcertSyncCore::Replication::FSyncControlState SyncControl = TSet<FConcertObjectInStreamID>{{ SenderStreamId, ObjectReplicator->TestObject }};
				const FConcertReplication_ChangeAuthority_Request Request { .ReleaseAuthority = { { ObjectReplicator->TestObject, {{ SenderStreamId }} } } };
				const FConcertReplication_ChangeAuthority_Response Response;
				
				TArray<FConcertObjectInStreamID> RemovedObjects;
				SyncControl.AppendChanges(
					Request,
					Response,
					[this](const FConcertObjectInStreamID&){ AddError(TEXT("No object should be added")); },
					[&RemovedObjects](const FConcertObjectInStreamID& Object){ RemovedObjects.Add(Object); }
					);
				
				TestTrue(TEXT("Removed TestObject"), RemovedObjects.Contains(FConcertObjectInStreamID{ SenderStreamId, ObjectReplicator->TestObject }));
				TestEqual(TEXT("Removed exactly 1 object"), RemovedObjects.Num(), 1);
			});
			
			It("Is correct with taking authority with FConcertReplication_ChangeAuthority_Request", [this]()
			{
				ConcertSyncCore::Replication::FSyncControlState SyncControl;
				const FConcertObjectInStreamID ObjectId{ SenderStreamId, ObjectReplicator->TestObject };
				const FConcertReplication_ChangeAuthority_Request Request { .TakeAuthority = {{ ObjectReplicator->TestObject, {{ SenderStreamId }}}}};
				const FConcertReplication_ChangeAuthority_Response Response { .SyncControl = {{{ ObjectId, true }}}};
				
				TArray<FConcertObjectInStreamID> AddedObjects;
				SyncControl.AppendChanges(
					Request,
					Response,
					[&AddedObjects](const FConcertObjectInStreamID& Object){ AddedObjects.Add(Object); },
					[this](const FConcertObjectInStreamID&){ AddError(TEXT("No object should be removed")); }
					);
				
				TestTrue(TEXT("Added TestObject"), AddedObjects.Contains(ObjectId));
				TestEqual(TEXT("Added exactly 1 object"), AddedObjects.Num(), 1);
			});

			It("Is correct after releasing authority with FConcertReplication_ChangeStream_Request", [this]()
			{
				ConcertSyncCore::Replication::FSyncControlState SyncControl = TSet<FConcertObjectInStreamID>{{ SenderStreamId, ObjectReplicator->TestObject }};
				FConcertReplication_ChangeStream_Request StreamChange;
				StreamChange.ObjectsToRemove.Add({ SenderStreamId, ObjectReplicator->TestObject });

				TArray<FConcertObjectInStreamID> RemovedObjects;
				SyncControl.AppendChanges(StreamChange, [&RemovedObjects](const FConcertObjectInStreamID& Object){ RemovedObjects.Add(Object); });
				
				TestTrue(TEXT("Removed TestObject"), RemovedObjects.Contains(FConcertObjectInStreamID{ SenderStreamId, ObjectReplicator->TestObject }));
				TestEqual(TEXT("Removed exactly 1 object"), RemovedObjects.Num(), 1);
			});
		});
	}
}
