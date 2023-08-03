// Copyright Epic Games, Inc. All Rights Reserved.

#include "Util/ConcertMocks.h"

#include "Replication/Data/ReplicationClientDescription.h"
#include "Replication/Data/ReplicationStreamDescription.h"
#include "Replication/IConcertClientReplicationBridge.h"
#include "Replication/IConcertClientReplicationManager.h"
#include "Replication/IConcertServerReplicationManager.h"
#include "Replication/Messages/ConcertReplicationHandshakeMessages.h"
#include "ReplicationTestInterface.h"
#include "Util/ConcertClientReplicationBridgeMock.h"

#include "Components/StaticMeshComponent.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

namespace UE::ConcertSyncTests
{
	/**
	 * Tests the handshake for joining and leaving a replication session.
	 */
	IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FJoinHandshakeTest, FConcertClientServerCommunicationTest, "Concert.Replication.JoinHandshake", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);
	bool FJoinHandshakeTest::RunTest(const FString& Parameters)
	{
		using namespace ConcertSyncClient::TestInterface;
		using namespace ConcertSyncServer::TestInterface;
		using namespace ConcertSyncClient::Replication;
		using namespace ConcertSyncServer::Replication;
		const FSoftObjectPath PathToSomeActorComponent(TEXT("/Game/Map.Map:PersistentLevel.StaticMeshActor0.StaticMeshComponent0"));
		const FConcertPropertyChain ForcedLodModelProperty = *FConcertPropertyChain::CreateFromPath(*UStaticMeshComponent::StaticClass(), { TEXT("ForcedLodModel") });
		const FConcertPropertyChain MinLODProperty = *FConcertPropertyChain::CreateFromPath(*UStaticMeshComponent::StaticClass(), { TEXT("MinLOD") });

		// 1. Init
		
		// Server
		InitServer();
		const TSharedPtr<IConcertServerSession>& ServerSession = GetServerSessionMock();
		const TSharedRef<IConcertServerReplicationManager> ServerReplicationManager = CreateServerReplicationManager(ServerSession.ToSharedRef());
		
		// Client
		FClientInfo& Client = ConnectClient();
		const TSharedRef<IConcertClientReplicationBridge> BridgeMock = MakeShared<FConcertClientReplicationBridgeMock>();
		const TSharedPtr<IConcertClientSession>& ClientSession = Client.ClientSessionMock;
		const TSharedRef<IConcertClientReplicationManager> ClientReplicationManager = CreateClientReplicationManager(ClientSession.ToSharedRef(), &BridgeMock.Get());
		FClientInfo& Client_Secondary = ConnectClient();
		const TSharedRef<IConcertClientReplicationBridge> BridgeMock_Secondary = MakeShared<FConcertClientReplicationBridgeMock>();
		const TSharedPtr<IConcertClientSession>& ClientSession_Secondary = Client_Secondary.ClientSessionMock;
		const TSharedRef<IConcertClientReplicationManager> ClientReplicationManager_Secondary = CreateClientReplicationManager(ClientSession_Secondary.ToSharedRef(), &BridgeMock_Secondary.Get());

		// Stream info
		FReplicationClientDescription ClientDescription;
		FReplicationStreamDescription StreamDescription;
		FReplicatedObjectInfo StaticMeshComponentInfo;
		StaticMeshComponentInfo.ClassPath = UStaticMeshComponent::StaticClass();
		StaticMeshComponentInfo.PropertySelection.ReplicatedProperties.Add(ForcedLodModelProperty);
		StreamDescription.BaseDescription.ReplicationMap.ReplicatedObjects.Add(PathToSomeActorComponent, StaticMeshComponentInfo);

		FReplicationStreamDescription DuplicateStreamDescription = StreamDescription;
		DuplicateStreamDescription.BaseDescription.Identifier = FGuid::NewGuid();
		
		FReplicationStreamDescription InvalidClassStreamDescription = StreamDescription;
		FReplicatedObjectInfo MissingClassInfo = StaticMeshComponentInfo;
		MissingClassInfo.ClassPath.Reset();
		InvalidClassStreamDescription.BaseDescription.ReplicationMap.ReplicatedObjects.Add(PathToSomeActorComponent, MissingClassInfo);

		
		// 2. Run

		// 2.1 Invalid configurations
		// 2.1.1 Duplicate properties
		FReplicationStreamDescription Invalid_DoublePropertyDescription = StreamDescription;
		Invalid_DoublePropertyDescription.BaseDescription.ReplicationMap.ReplicatedObjects[PathToSomeActorComponent].PropertySelection.ReplicatedProperties.Add(ForcedLodModelProperty);
		ClientReplicationManager->JoinReplicationSession({ ClientDescription, { Invalid_DoublePropertyDescription } })
			.Next([&](const FJoinReplicatedSessionResult& Result)
			{
				TestTrue(TEXT("Cannot contain same properties twice"), Result.ErrorCode == EJoinReplicationErrorCode::DuplicateProperty);
			});
		// 2.1.2 Duplicate stream identifier
		ClientReplicationManager->JoinReplicationSession({ ClientDescription, { StreamDescription, StreamDescription } })
			.Next([&](const FJoinReplicatedSessionResult& Result)
			{
				TestTrue(TEXT("Cannot contain stream ID twice"), Result.ErrorCode == EJoinReplicationErrorCode::ConflictingStreamId);
			});
		// 2.1.3 Miss class path
		ClientReplicationManager->JoinReplicationSession({ ClientDescription, { InvalidClassStreamDescription } })
			.Next([&](const FJoinReplicatedSessionResult& Result)
			{
				TestTrue(TEXT("Cannot contain null classes"), Result.ErrorCode == EJoinReplicationErrorCode::InvalidClass);
			});

		// 2.2 Valid join
		ClientReplicationManager->JoinReplicationSession({ ClientDescription, { StreamDescription } })
			.Next([&](const FJoinReplicatedSessionResult& Result)
			{
				TestTrue(TEXT("Join with valid args"), Result.ErrorCode == EJoinReplicationErrorCode::Success);
			});
		// 2.3 No joining twice
		AddExpectedError(TEXT("JoinReplicationSession requested while already in a session"), EAutomationExpectedErrorFlags::Contains); // Not pretty, but otherwise this test fails due to logged warning
		ClientReplicationManager->JoinReplicationSession({ ClientDescription, { StreamDescription } })
			.Next([&](const FJoinReplicatedSessionResult& Result)
			{
				TestTrue(TEXT("Cannot join twice"), Result.ErrorCode == EJoinReplicationErrorCode::AlreadyInSession);
			});
		// 2.4 Rejoining
		ClientReplicationManager->LeaveReplicationSession();
		ClientReplicationManager->JoinReplicationSession({ ClientDescription, { StreamDescription } })
			.Next([&](const FJoinReplicatedSessionResult& Result)
			{
				TestTrue(TEXT("Re-join session"), Result.ErrorCode == EJoinReplicationErrorCode::Success);
			});

		
		// 2.5 Second client is not allowed replicate the same properties as the first client
		FReplicationStreamDescription DuplicateProperties = StreamDescription;
		DuplicateProperties.BaseDescription.Identifier = FGuid::NewGuid();
		ClientReplicationManager_Secondary->JoinReplicationSession({ ClientDescription, { DuplicateProperties } })
			.Next([&](const FJoinReplicatedSessionResult& Result)
			{
				TestTrue(TEXT("Clients cannot overlap properties"), Result.ErrorCode == EJoinReplicationErrorCode::ConflictingAuthority);
			});
		// 2.6 Second client is allowed to replicate different properties on the same object
		FReplicationStreamDescription NonOverlappingProperties;
		NonOverlappingProperties.BaseDescription.Identifier = FGuid::NewGuid();
		FReplicatedObjectInfo StaticMeshComponentInfo_Secondary;
		StaticMeshComponentInfo_Secondary.ClassPath = UStaticMeshComponent::StaticClass();
		StaticMeshComponentInfo_Secondary.PropertySelection.ReplicatedProperties.Add(MinLODProperty);
		NonOverlappingProperties.BaseDescription.ReplicationMap.ReplicatedObjects.Add(PathToSomeActorComponent, StaticMeshComponentInfo_Secondary);
		ClientReplicationManager_Secondary->JoinReplicationSession({ ClientDescription, { NonOverlappingProperties } })
			.Next([&](const FJoinReplicatedSessionResult& Result)
			{
				TestTrue(TEXT("Two clients can replicate differing properties on the same object"), Result.ErrorCode == EJoinReplicationErrorCode::Success);
			});
		
		return true;
	}
}
