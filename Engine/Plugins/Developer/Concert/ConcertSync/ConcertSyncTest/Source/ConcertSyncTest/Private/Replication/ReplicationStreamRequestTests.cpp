// Copyright Epic Games, Inc. All Rights Reserved.

#include "Util/ClientServerCommunicationTest.h"

#include "Replication/Data/ReplicationStreamDescription.h"
#include "Replication/IConcertClientReplicationManager.h"
#include "TestReflectionObject.h"
#include "Util/SendReceiveObjectTestBase.h"

#include "Components/StaticMeshComponent.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

namespace UE::ConcertSyncTests::Replication::Stream
{
	/**
	 * Let's receiving client query the server what the sending client is sending. Checks
	 * - the reported streams,
	 * - that the authority updates,
	 * - the EConcertQueryClientStreamFlags flags
	 */
	IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FQueryOtherClientStreams, FSendReceiveObjectTestBase, "Concert.Replication.QueryClientStream", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);
	bool FQueryOtherClientStreams::RunTest(const FString& Parameters)
	{
		// 1. Init
		SetUpClientAndServer();
		
		const FGuid SenderEndpointId = Client_Sender->ClientSessionMock->GetSessionClientEndpointId();
		const FSoftObjectPath TestObjectPath{ TestObject };
		ConcertSyncClient::Replication::FClientQueryRequest Request;
		Request.ClientEndpointIds = { SenderEndpointId };
		auto TestReplicationMapContent = [this, &SenderEndpointId](const ConcertSyncClient::Replication::FClientQueryResponse& Response)
		{
			const FReplicationClientQueriedInfo* Info = Response.ClientInfo.Find(SenderEndpointId);
			if (!Info)
			{
				AddError(TEXT("No info about sending client!"));
				return;
			}
				
			TestEqual(TEXT("Only received info about requested client"), Response.ClientInfo.Num(), 1);
			TestEqual(TEXT("Exactly one stream data"), Info->Streams.Num(), 1);
			if (Info->Streams.IsEmpty())
			{
				return;
			}
				
			const FSharedReplicationStreamDescription& StreamDescription = Info->Streams[0];
			TestEqual(TEXT("StreamDescription->Identifier correct"), StreamDescription.Identifier, SenderStreamId);
			TestEqual(TEXT("StreamDescription->ReplicationMap has exactly 1 object"), StreamDescription.ReplicationMap.ReplicatedObjects.Num(), 1);

			const FObjectReplicationMap ExpectedReplicationMap = CreateSenderArgs().Streams[0].BaseDescription.ReplicationMap;
			TestEqual(TEXT("Registered and reported replication maps match"), StreamDescription.ReplicationMap, ExpectedReplicationMap);
		};
		
		// 2.1 Request before taking authority
		bool bReceivedFirstQueryResponse = false;
		ClientReplicationManager_Receiver->QueryClientInfo(Request)
			.Next([this, &SenderEndpointId, &TestReplicationMapContent, &bReceivedFirstQueryResponse](ConcertSyncClient::Replication::FClientQueryResponse&& Response) mutable
			{
				bReceivedFirstQueryResponse = true;
				TestReplicationMapContent(Response);
				if (const FReplicationClientQueriedInfo* Info = Response.ClientInfo.Find(SenderEndpointId))
				{
					TestEqual(TEXT("Contains no authority data"), Info->Authority.Num(), 0);
				}
			});
		TestTrue(TEXT("Received query response (before taking authority)"), bReceivedFirstQueryResponse);

		// 2.2 Request after taking authority
		bool bSenderReceivedResponse = false;
		ClientReplicationManager_Sender->TakeAuthorityOver({ TestObjectPath })
			.Next([this, &bSenderReceivedResponse](const ConcertSyncClient::Replication::FAuthorityChangeResponse& Response) mutable
			{
				bSenderReceivedResponse = true;
				TestEqual(TEXT("No rejection taking authority"), Response.RejectedObjects.Num(), 0); 
			});

		bool bReceivedSecondQueryResponse = false;
		ClientReplicationManager_Receiver->QueryClientInfo(Request)
			.Next([this, &SenderEndpointId, &TestReplicationMapContent, &TestObjectPath, &bReceivedSecondQueryResponse](ConcertSyncClient::Replication::FClientQueryResponse&& Response) mutable
			{
				bReceivedSecondQueryResponse = true;
				TestReplicationMapContent(Response);
				
				const FReplicationClientQueriedInfo* Info = Response.ClientInfo.Find(SenderEndpointId);
				if (!Info || Info->Authority.IsEmpty())
				{
					AddError(TEXT("Missing authority data"));
					return;
				}

				TestEqual(TEXT("Exactly 1 authority stream"), Info->Authority.Num(), 1);
				const FReplicationAuthorityInfo& AuthorityInfo = Info->Authority[0];
				TestEqual(TEXT("Authority stream ID matches registered stream ID"), AuthorityInfo.StreamId, SenderStreamId);
				TestEqual(TEXT("Has authority over exactly 1 object"), AuthorityInfo.AuthoredObjects.Num(), 1);
				TestTrue(TEXT("Has authority over registered object"), AuthorityInfo.AuthoredObjects.Contains(TestObjectPath));
			});
		TestTrue(TEXT("Received response to taking authority"), bSenderReceivedResponse);
		TestTrue(TEXT("Received query response (after taking authority)"), bReceivedSecondQueryResponse);

		// 2.3 SkipStreamInfo
		bool bReceivedResponse_SkipStreamInfo = false;
		Request.QueryFlags = EConcertQueryClientStreamFlags::SkipStreamInfo;
		ClientReplicationManager_Receiver->QueryClientInfo(Request)
			.Next([this, &SenderEndpointId, &bReceivedResponse_SkipStreamInfo](ConcertSyncClient::Replication::FClientQueryResponse&& Response) mutable
			{
				bReceivedResponse_SkipStreamInfo = true;
				if (const FReplicationClientQueriedInfo* Info = Response.ClientInfo.Find(SenderEndpointId))
				{
					TestEqual(TEXT("SkipStreamInfo > No stream data"), Info->Streams.Num(), 0);
				}
			});
		TestTrue(TEXT("bReceivedResponse_SkipStreamInfo"), bReceivedResponse_SkipStreamInfo);
		
		// 2.4 SkipProperties
		bool bReceivedResponse_SkipProperties = false;
		Request.QueryFlags = EConcertQueryClientStreamFlags::SkipProperties;
		ClientReplicationManager_Receiver->QueryClientInfo(Request)
			.Next([this, &SenderEndpointId, &TestObjectPath, &bReceivedResponse_SkipProperties](ConcertSyncClient::Replication::FClientQueryResponse&& Response) mutable
			{
				bReceivedResponse_SkipProperties = true;
				const FReplicationClientQueriedInfo* Info = Response.ClientInfo.Find(SenderEndpointId);
				if (!Info || Info->Streams.IsEmpty())
				{
					AddError(TEXT("SkipProperties > No stream info"));
					return;
				}

				const FReplicatedObjectInfo* ObjectInfo = Info->Streams[0].ReplicationMap.ReplicatedObjects.Find(TestObjectPath);
				if (!ObjectInfo)
				{
					AddError(TEXT("SkipProperties > No object info"));
					return;
				}
				TestEqual(TEXT("SkipProperties > No property data"), ObjectInfo->PropertySelection.ReplicatedProperties.Num(), 0);
			});
		TestTrue(TEXT("bReceivedResponse_SkipProperties"), bReceivedResponse_SkipProperties);
		
		// 2.5 SkipAuthority
		bool bReceivedResponse_SkipAuthority = false;
		Request.QueryFlags = EConcertQueryClientStreamFlags::SkipAuthority;
        ClientReplicationManager_Receiver->QueryClientInfo(Request)
        	.Next([this, &SenderEndpointId, &bReceivedResponse_SkipAuthority](ConcertSyncClient::Replication::FClientQueryResponse&& Response) mutable
        	{
        		bReceivedResponse_SkipAuthority = true;
        		if (const FReplicationClientQueriedInfo* Info = Response.ClientInfo.Find(SenderEndpointId))
        		{
        			TestEqual(TEXT("SkipAuthority > No authority data"), Info->Authority.Num(), 0);
        		}
        	});
		TestTrue(TEXT("bReceivedResponse_SkipAuthority"), bReceivedResponse_SkipAuthority);
		
		return true;
	}
}
