// Copyright Epic Games, Inc. All Rights Reserved.

#include "Replication/Util/Spec/ReplicationClient.h"
#include "Replication/Util/Spec/ReplicationServer.h"
#include "Replication/Util/Spec/ObjectTestReplicator.h"

#include "Misc/AutomationTest.h"
#include "Replication/Messages/PutState.h"

namespace UE::ConcertSyncTests::Replication::ChangeClients
{
	BEGIN_DEFINE_SPEC(FPutStateMiscSpec, "Editor.Concert.Replication.PutState.Misc", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
		TUniquePtr<FObjectTestReplicator> ObjectReplicator;

		TUniquePtr<FReplicationServer> Server;
		FReplicationClient* Sender = nullptr;
	
		const FGuid StreamId = FGuid::NewGuid();
	END_DEFINE_SPEC(FPutStateMiscSpec);

	/** This tests misc workflows and cases with FConcertReplication_PutState_Request that do not really fit well with other specs. */
	void FPutStateMiscSpec::Define()
	{
		BeforeEach([this]
		{
			ObjectReplicator = MakeUnique<FObjectTestReplicator>();
			Server = MakeUnique<FReplicationServer>(*this);
			Sender = &Server->ConnectClient();

			Sender->JoinReplication();
		});
		AfterEach([this]
		{
			Server.Reset();
			ObjectReplicator.Reset();
		});

		// This was a bug where the server would not remove the stream from the client.
		Describe("When a put request removes client state", [this]
		{
			BeforeEach([this]
			{
				IConcertClientReplicationManager& ReplicationManager = Sender->GetClientReplicationManager();
				ReplicationManager.ChangeStream({ .StreamsToAdd = { ObjectReplicator->CreateStream(StreamId) } });
				ReplicationManager.PutClientState({ .NewStreams = { { Sender->GetEndpointId(), {} } } });
			});
			
			It("The client state can create a new stream", [this]
			{
				bool bReceivedResponse = false;
				Sender->GetClientReplicationManager()
					.ChangeStream({ .StreamsToAdd = { ObjectReplicator->CreateStream(StreamId) } })
					.Next([this, &bReceivedResponse](FConcertReplication_ChangeStream_Response&& Response)
					{
						bReceivedResponse = true;
						TestTrue(TEXT("Success"), Response.IsSuccess());
					});
				TestTrue(TEXT("bReceivedResponse"), bReceivedResponse);
			});

			It("The stream has been fully deleted from the session", [this]
			{
				bool bReceivedResponse = false;
				Sender->GetClientReplicationManager()
					.QueryClientInfo({ .ClientEndpointIds = { Sender->GetEndpointId() } })
					.Next([this, &bReceivedResponse](FConcertReplication_QueryReplicationInfo_Response&& Response)
					{
						bReceivedResponse = true;

						const FConcertQueriedClientInfo* ClientInfo = Response.ClientInfo.Find(Sender->GetEndpointId());
						TestTrue(TEXT("No streams"), ClientInfo && ClientInfo->Streams.IsEmpty());
					});
				TestTrue(TEXT("bReceivedResponse"), bReceivedResponse);
			});
		});
	}
}
