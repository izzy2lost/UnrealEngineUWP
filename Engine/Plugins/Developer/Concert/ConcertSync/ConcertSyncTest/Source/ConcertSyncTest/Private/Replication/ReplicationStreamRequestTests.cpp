// Copyright Epic Games, Inc. All Rights Reserved.

#include "Util/ClientServerCommunicationTest.h"

#include "Replication/Data/ReplicationStreamDescription.h"
#include "Replication/IConcertClientReplicationManager.h"
#include "TestReflectionObject.h"
#include "Util/SendReceiveGenericStreamTestBase.h"
#include "Util/SendReceiveObjectTestBase.h"

#include "Misc/AutomationTest.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/Package.h"

namespace UE::ConcertSyncTests::Replication::Stream
{
	/**
	 * Let's receiving client query the server what the sending client is sending. Checks
	 * - the reported streams,
	 * - that the authority updates,
	 * - the EConcertQueryClientStreamFlags flags
	 */
	IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FQueryOtherClientStreams, FSendReceiveObjectTestBase, "Concert.Replication.Stream.QueryClientStream", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);
	bool FQueryOtherClientStreams::RunTest(const FString& Parameters)
	{
		// 1. Init
		SetUpClientAndServer();
		
		const FGuid SenderEndpointId = Client_Sender->ClientSessionMock->GetSessionClientEndpointId();
		const FSoftObjectPath TestObjectPath{ TestObject };
		FConcertReplication_QueryReplicationInfo_Request Request;
		Request.ClientEndpointIds = { SenderEndpointId };
		auto TestReplicationMapContent = [this, &SenderEndpointId](const FConcertReplication_QueryReplicationInfo_Response& Response)
		{
			TestTrue(TEXT("ErrorCode == Handled"), Response.ErrorCode == EReplicationResponseErrorCode::Handled);
			
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
			.Next([this, &SenderEndpointId, &TestReplicationMapContent, &bReceivedFirstQueryResponse](FConcertReplication_QueryReplicationInfo_Response&& Response) mutable
			{
				bReceivedFirstQueryResponse = true;
				TestTrue(TEXT("ErrorCode == Handled"), Response.ErrorCode == EReplicationResponseErrorCode::Handled);
				
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
			.Next([this, &bSenderReceivedResponse](const FConcertReplication_ChangeAuthority_Response& Response) mutable
			{
				bSenderReceivedResponse = true;
				TestEqual(TEXT("No rejection taking authority"), Response.RejectedObjects.Num(), 0); 
				TestTrue(TEXT("ErrorCode == Handled"), Response.ErrorCode == EReplicationResponseErrorCode::Handled);
			});
		TestTrue(TEXT("Received response taking authority"), bSenderReceivedResponse);

		bool bReceivedSecondQueryResponse = false;
		ClientReplicationManager_Receiver->QueryClientInfo(Request)
			.Next([this, &SenderEndpointId, &TestReplicationMapContent, &TestObjectPath, &bReceivedSecondQueryResponse](FConcertReplication_QueryReplicationInfo_Response&& Response) mutable
			{
				bReceivedSecondQueryResponse = true;
				TestTrue(TEXT("ErrorCode == Handled"), Response.ErrorCode == EReplicationResponseErrorCode::Handled);
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
			.Next([this, &SenderEndpointId, &bReceivedResponse_SkipStreamInfo](FConcertReplication_QueryReplicationInfo_Response&& Response) mutable
			{
				bReceivedResponse_SkipStreamInfo = true;
				TestTrue(TEXT("ErrorCode == Handled"), Response.ErrorCode == EReplicationResponseErrorCode::Handled);
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
			.Next([this, &SenderEndpointId, &TestObjectPath, &bReceivedResponse_SkipProperties](FConcertReplication_QueryReplicationInfo_Response&& Response) mutable
			{
				bReceivedResponse_SkipProperties = true;
				const FReplicationClientQueriedInfo* Info = Response.ClientInfo.Find(SenderEndpointId);
				TestTrue(TEXT("ErrorCode == Handled"), Response.ErrorCode == EReplicationResponseErrorCode::Handled);
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
        	.Next([this, &SenderEndpointId, &bReceivedResponse_SkipAuthority](FConcertReplication_QueryReplicationInfo_Response&& Response) mutable
        	{
        		bReceivedResponse_SkipAuthority = true;
				TestTrue(TEXT("ErrorCode == Handled"), Response.ErrorCode == EReplicationResponseErrorCode::Handled);
        		if (const FReplicationClientQueriedInfo* Info = Response.ClientInfo.Find(SenderEndpointId))
        		{
        			TestEqual(TEXT("SkipAuthority > No authority data"), Info->Authority.Num(), 0);
        		}
        	});
		TestTrue(TEXT("bReceivedResponse_SkipAuthority"), bReceivedResponse_SkipAuthority);
		
		return true;
	}

	/**
	 * Base test for testing ChangeStream requests.
	 * 
	 * While there is no sending nor receiving of replicated data, subclasses will still refer to the clients as "Sender"
	 * and "Receiver".
	 */
	class FChangeStreamsTestBase : public FSendReceiveGenericTestBase
	{
	public:

		FChangeStreamsTestBase(const FString& InName, const bool bInComplexTask)
			: FSendReceiveGenericTestBase(InName, bInComplexTask)
		{}

	protected:
		
		ConcertSyncClient::Replication::FJoinReplicatedSessionArgs SenderArgs;
		ConcertSyncClient::Replication::FJoinReplicatedSessionArgs ReceiverArgs;

		//~ Begin FSendReceiveTestBase Interface
		virtual ConcertSyncClient::Replication::FJoinReplicatedSessionArgs CreateSenderArgs() override { return SenderArgs; }
		virtual ConcertSyncClient::Replication::FJoinReplicatedSessionArgs CreateReceiverArgs() override { return ReceiverArgs; }
		//~ End FSendReceiveTestBase Interfac

		static TTuple<FGuid, FReplicationStreamDescription> CreateFloatPropertyStream(const UTestReflectionObject& TestObject)
		{
			const FGuid NewStreamId = FGuid::NewGuid();
			FConcertPropertySelection Properties;
			AddFloatProperty(Properties);
			
			const FReplicatedObjectInfo AllProperties { TestObject.GetClass(), MoveTemp(Properties) };
			FReplicationStreamDescription SendingStream;
			SendingStream.BaseDescription.Identifier = NewStreamId;
			SendingStream.BaseDescription.ReplicationMap.ReplicatedObjects.Add(&TestObject, AllProperties);
			return { NewStreamId, SendingStream };
		}

		static TTuple<FGuid, FReplicationStreamDescription> CreateVectorPropertyStream(const UTestReflectionObject& TestObject)
		{
			const FGuid NewStreamId = FGuid::NewGuid();
			FConcertPropertySelection Properties;
			AddVectorProperty(Properties);
			
			const FReplicatedObjectInfo AllProperties { TestObject.GetClass(), MoveTemp(Properties) };
			FReplicationStreamDescription SendingStream;
			SendingStream.BaseDescription.Identifier = NewStreamId;
			SendingStream.BaseDescription.ReplicationMap.ReplicatedObjects.Add(&TestObject, AllProperties);
			return { NewStreamId, SendingStream };
		}

		static FConcertPropertySelection& GetPropertySelection(FReplicationStreamDescription& Stream, const UTestReflectionObject& TestObject)
		{
			return Stream.BaseDescription.ReplicationMap.ReplicatedObjects.FindOrAdd(&TestObject).PropertySelection;
		}

		static void AddFloatProperty(FConcertPropertySelection& PropertySelection)
		{
			const FProperty* FloatProperty = UTestReflectionObject::StaticClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UTestReflectionObject, Float));
			const FConcertPropertyChain PropertyChain(nullptr, *FloatProperty);
			PropertySelection.ReplicatedProperties.Add(PropertyChain);
		}

		static void AddVectorProperty(FConcertPropertySelection& PropertySelection)
		{
			PropertySelection.ReplicatedProperties.Add(*FConcertPropertyChain::CreateFromPath(*UTestReflectionObject::StaticClass(), { TEXT("Vector") }));
			PropertySelection.ReplicatedProperties.Add(*FConcertPropertyChain::CreateFromPath(*UTestReflectionObject::StaticClass(), { TEXT("Vector"), TEXT("X") }));
		}

		/** Sends a ChangeStream request and validates that ExpectedStreams is equal to the registered streams. */
		void ChangeStreamForSenderClientAndValidate(const FString& InTestName, const FConcertReplication_ChangeStream_Request& Request, const TArray<FSharedReplicationStreamDescription>& ExpectedStreams)
		{
			using namespace ConcertSyncClient::Replication;
			
			bool bModifiedInitialStream = false;
			ClientReplicationManager_Sender->ChangeStream(Request)
				.Next([this, &InTestName, &bModifiedInitialStream](FConcertReplication_ChangeStream_Response&& Response)
				{
					bModifiedInitialStream = true;
					TestTrue(TEXT("ErrorCode == Handled"), Response.ErrorCode == EReplicationResponseErrorCode::Handled);
					TestTrue(FString::Printf(TEXT("%s > Modify Stream > Success"), *InTestName), Response.IsSuccess());
				});
			TestTrue(FString::Printf(TEXT("%s > Modify Stream > Received response"), *InTestName), bModifiedInitialStream);
			ValidateSenderClientStreams(InTestName, ExpectedStreams);
		}
		
		void ValidateSenderClientStreams(const FString& InTestName, const TArray<FSharedReplicationStreamDescription>& Streams)
		{
			using namespace ConcertSyncClient::Replication;

			// Test that remote clients see the receive the same as Streams
			bool bQueriedClientInfo = false;
			const FGuid SenderId = Client_Sender->ClientSessionMock->GetSessionClientEndpointId();
			ClientReplicationManager_Receiver->QueryClientInfo({{ { SenderId } }})
				.Next([this, &InTestName, &bQueriedClientInfo, &Streams, &SenderId](FConcertReplication_QueryReplicationInfo_Response&& Response)
				{
					bQueriedClientInfo = true;
					TestTrue(TEXT("ErrorCode == Handled"), Response.ErrorCode == EReplicationResponseErrorCode::Handled);
					const FReplicationClientQueriedInfo* Info = Response.ClientInfo.Find(SenderId);
					const bool bRegisteredAndQueriedStreamsAreEqual = Info->Streams == Streams;
					TestTrue(FString::Printf(TEXT("%s > Registered and queried streams are equal"), *InTestName), bRegisteredAndQueriedStreamsAreEqual);
				});

			// Validate the local client's cache equals  Streams
			const TArray<FReplicationStreamDescription> LocalStreams = ClientReplicationManager_Sender->GetRegisteredStreams();
			TArray<FSharedReplicationStreamDescription> TransformedLocalStreams;
			Algo::Transform(LocalStreams, TransformedLocalStreams, [](const FReplicationStreamDescription& Stream){ return Stream.BaseDescription; });
			TestEqual(FString::Printf(TEXT("%s > Local streams match registered streams"), *InTestName), TransformedLocalStreams, Streams);
		}
	};
	
	/** Simple case of FConcertChangeStream_Request where the requesting client has authority and thus cannot generate any authority conflicts. */
	IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FSimpleChangeStream, FChangeStreamsTestBase, "Concert.Replication.Stream.SimpleChangeStream", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);
	bool FSimpleChangeStream::RunTest(const FString& Parameters)
	{
		using namespace ConcertSyncClient::Replication;

		// 1. Start up client with float property stream
		UTestReflectionObject* TestObject = NewObject<UTestReflectionObject>(GetTransientPackage());
		auto[InitialStreamId, InitialStream] = CreateFloatPropertyStream(*TestObject);
		SenderArgs.Streams = { InitialStream };
		SetUpClientAndServer();

		// 2.1 Modify existing stream
		FConcertPropertySelection& ChangedInitialProperties = GetPropertySelection(InitialStream, *TestObject);
		AddVectorProperty(ChangedInitialProperties);
		FConcertReplication_ChangeStream_Request ModifyRequest;
		ModifyRequest.ObjectsToPut.Add({ InitialStreamId, TestObject }, { ChangedInitialProperties });
		ChangeStreamForSenderClientAndValidate(TEXT("ModifyRequest"), ModifyRequest, { InitialStream.BaseDescription });

		// 2.2 Add new stream
		auto[DynamicStreamId, DynamicStream] = CreateFloatPropertyStream(*TestObject);
		FConcertReplication_ChangeStream_Request DynamicCreationRequest;
		DynamicCreationRequest.StreamsToAdd.Add(DynamicStream.Pack());
		ChangeStreamForSenderClientAndValidate(TEXT("DynamicCreationRequest"), DynamicCreationRequest, { InitialStream.BaseDescription, DynamicStream.BaseDescription });

		// 2.3 Remove stream
		FConcertReplication_ChangeStream_Request RemoveStreamRequest;
		RemoveStreamRequest.StreamsToRemove.Add(InitialStreamId);
		ChangeStreamForSenderClientAndValidate(TEXT("RemoveStreamRequest"), RemoveStreamRequest, { DynamicStream.BaseDescription });

		// 2.4 Remove object > removes stream implicitly
		FConcertReplication_ChangeStream_Request RemoveObjectRequest;
		RemoveObjectRequest.ObjectsToRemove.Add({ DynamicStreamId, TestObject });
		ChangeStreamForSenderClientAndValidate(TEXT("RemoveObjectRequest"), RemoveObjectRequest, {});
		
		return true;
	}

	/** Case of FConcertChangeStream_Request in which requester has authority over an object and creates two streams which have authority over overlapping properties. This is allowed. */
	IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FChangeStreamWithOverlappingProperties, FChangeStreamsTestBase, "Concert.Replication.Stream.ChangeStreamWithOverlappingProperties", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);
	bool FChangeStreamWithOverlappingProperties::RunTest(const FString& Parameters)
	{
		using namespace ConcertSyncClient::Replication;

		// 1. Start client without any streams
		UTestReflectionObject* TestObject = NewObject<UTestReflectionObject>(GetTransientPackage());
		SetUpClientAndServer();
		
		// 2.1 Add stream with float property
		auto[FloatStreamID, FloatStream] = CreateFloatPropertyStream(*TestObject);
		FConcertReplication_ChangeStream_Request CreateFloatStreamRequest;
		CreateFloatStreamRequest.StreamsToAdd.Add(FloatStream.Pack());
		ChangeStreamForSenderClientAndValidate(TEXT("CreateFloatStreamRequest"), CreateFloatStreamRequest, { FloatStream.BaseDescription });
		// 2.2 Add a stream with vector property
		auto[VectorFloatStreamID, VectorFloatStream] = CreateVectorPropertyStream(*TestObject);
		FConcertReplication_ChangeStream_Request CreateVectorFloatStreamRequest; 
		CreateVectorFloatStreamRequest.StreamsToAdd.Add(VectorFloatStream.Pack());
		ChangeStreamForSenderClientAndValidate(TEXT("CreateVectorFloatStreamRequest"), CreateVectorFloatStreamRequest, { FloatStream.BaseDescription, VectorFloatStream.BaseDescription });

		// 2.3 Take authority over both streams
		bool bTookAuthority = false;
		FConcertReplication_ChangeAuthority_Request TakeAuthorityRequest;
		TakeAuthorityRequest.TakeAuthority.Add(TestObject, FConcertStreamArray{{ FloatStreamID, VectorFloatStreamID }});
		ClientReplicationManager_Sender->RequestAuthorityChange({ TakeAuthorityRequest })
			.Next([this, &bTookAuthority](FConcertReplication_ChangeAuthority_Response&& Response)
			{
				bTookAuthority = true;
				TestTrue(TEXT("ErrorCode == Handled"), Response.ErrorCode == EReplicationResponseErrorCode::Handled);
				TestTrue(TEXT("Authority request > Success"), Response.RejectedObjects.IsEmpty());
			});
		TestTrue(TEXT("Authority request > Received response"), bTookAuthority);

		// 2.4 Appending the overlapping float property works
		FConcertReplication_ChangeStream_Request AppendFloatRequest;
		FConcertPropertySelection& NewSelection = GetPropertySelection(VectorFloatStream, *TestObject);
		AddFloatProperty(NewSelection);
		AppendFloatRequest.ObjectsToPut.Add(FObjectInStreamID{ VectorFloatStreamID, TestObject }, FConcertReplication_ChangeStream_PutObject{ NewSelection });
		ChangeStreamForSenderClientAndValidate(TEXT("AppendFloatRequest"), AppendFloatRequest, { FloatStream.BaseDescription, VectorFloatStream.BaseDescription });
		
		return true;
	}

	/**
	 * The requester and client have authority over separate properties on the same object.
	 * The request would cause the requester's existing authority to overlap with the other client, which is rejected.
	 */
	IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FRejectChangeStreamWithConflictingAuthority, FChangeStreamsTestBase, "Concert.Replication.Stream.RejectChangeStreamWithConflictingAuthority", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);
	bool FRejectChangeStreamWithConflictingAuthority::RunTest(const FString& Parameters)
	{
		using namespace ConcertSyncClient::Replication;

		// 1. Start client without any streams
		UTestReflectionObject* TestObject = NewObject<UTestReflectionObject>(GetTransientPackage());
		SetUpClientAndServer();
		
		// 2.1 Add stream with float property
		auto[SenderStreamID, SenderStream] = CreateFloatPropertyStream(*TestObject);
		FConcertReplication_ChangeStream_Request CreateFloatStreamRequest;
		CreateFloatStreamRequest.StreamsToAdd.Add(SenderStream.Pack());
		ChangeStreamForSenderClientAndValidate(TEXT("CreateFloatStreamRequest"), CreateFloatStreamRequest, { SenderStream.BaseDescription });

		// 2.2 Have Sender take authority over the float property
		bool bSenderTookAuthority = false;
		ClientReplicationManager_Sender->TakeAuthorityOver({ TestObject })
			.Next([this, &bSenderTookAuthority](const FConcertReplication_ChangeAuthority_Response& Response) mutable
			{
				bSenderTookAuthority = true;
				TestTrue(TEXT("ErrorCode == Handled"), Response.ErrorCode == EReplicationResponseErrorCode::Handled);
				TestEqual(TEXT("Sender > No rejection taking authority"), Response.RejectedObjects.Num(), 0); 
			});
		TestTrue(TEXT("Sender > Received response taking authority"), bSenderTookAuthority);

		// 2.3 Have Receiver create vector stream
		auto[ReceiverStreamID, ReceiverStream] = CreateVectorPropertyStream(*TestObject);
		FConcertReplication_ChangeStream_Request CreateVectorStreamRequest; 
		CreateVectorStreamRequest.StreamsToAdd.Add(ReceiverStream.Pack());
		bool bAddedVectorStream = false;
		ClientReplicationManager_Receiver->ChangeStream(CreateVectorStreamRequest)
			.Next([this, &bAddedVectorStream](FConcertReplication_ChangeStream_Response&& Response)
			{
				bAddedVectorStream = true;
				TestTrue(TEXT("ErrorCode == Handled"), Response.ErrorCode == EReplicationResponseErrorCode::Handled);
				TestTrue(TEXT("Vector Stream > Success"), Response.IsSuccess());
			});
		TestTrue(TEXT("Vector Stream> Received response"), bAddedVectorStream);
		// For simplicity, we don't do any further validation that other clients can see the change
		
		// 2.4 Have Receiver take authority over vector stream
		bool bReceiverTookAuthority = false;
		ClientReplicationManager_Receiver->TakeAuthorityOver({ TestObject })
			.Next([this, &bReceiverTookAuthority](const FConcertReplication_ChangeAuthority_Response& Response) mutable
			{
				bReceiverTookAuthority = true;
				TestTrue(TEXT("ErrorCode == Handled"), Response.ErrorCode == EReplicationResponseErrorCode::Handled);
				TestEqual(TEXT("Receiver > No rejection taking authority"), Response.RejectedObjects.Num(), 0); 
			});
		TestTrue(TEXT("Receiver > Received response taking authority"), bReceiverTookAuthority);

		// 2.5 Receiver is rejected from modifying the stream to include the Float property because the Sender has authority over it
		const FObjectInStreamID TestObjectInReceiverStreamId { ReceiverStreamID, TestObject };
		FConcertReplication_ChangeStream_Request AddFloatToStreamRequest;
		FConcertPropertySelection NewSelection = GetPropertySelection(ReceiverStream, *TestObject);
		AddFloatProperty(NewSelection);
		AddFloatToStreamRequest.ObjectsToPut.Add(TestObjectInReceiverStreamId, { NewSelection });
		bool bReceivedResponseAddingFloat = false;
		
		// Server logs a warning when rejecting - avoid the test being marked with a warning.
		AddExpectedError(TEXT("Rejecting ChangeStream"));
		ClientReplicationManager_Receiver->ChangeStream(AddFloatToStreamRequest)
			.Next([this, TestObject, SenderStreamID = SenderStreamID, &TestObjectInReceiverStreamId, &bReceivedResponseAddingFloat](FConcertReplication_ChangeStream_Response&& Response)
			{
				bReceivedResponseAddingFloat = true;
				TestTrue(TEXT("ErrorCode == Handled"), Response.ErrorCode == EReplicationResponseErrorCode::Handled);
				TestEqual(TEXT("Append Float > 1 conflict"), Response.AuthorityConflicts.Num(), 1);
				if (const FReplicatedObjectId* ConflictingObject = Response.AuthorityConflicts.Find(TestObjectInReceiverStreamId))
				{
					TestEqual(TEXT("Append float > Conflict > Sender Stream correct"), ConflictingObject->StreamId, SenderStreamID);
					TestEqual(TEXT("Append float > Conflict > Object correct"), ConflictingObject->Object, FSoftObjectPath(TestObject));
					TestEqual(TEXT("Append float > Conflict > Endpoint ID correct"), ConflictingObject->SenderEndpointId, Client_Sender->ClientSessionMock->GetSessionClientEndpointId());
				}
				else
				{
					AddError(TEXT("Expected to find an authority object conflict for TestObject"));
				}
				TestTrue(TEXT("Append Float > Failure"), Response.IsFailure());
			});
		TestTrue(TEXT("Append Float > Received response"), bReceivedResponseAddingFloat);
		
		return true;  
	}

	/** Tests that no changes are made if a sub-step of FConcertChangeStream_Request causes an error. */
	IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FChangeStreamIsAtmoic, FChangeStreamsTestBase, "Concert.Replication.Stream.ChangeStreamIsAtmoic", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);
	bool FChangeStreamIsAtmoic::RunTest(const FString& Parameters)
	{
		using namespace ConcertSyncClient::Replication;

		// 1. Start client without any streams
		UTestReflectionObject* TestObject = NewObject<UTestReflectionObject>(GetTransientPackage());
		SetUpClientAndServer();
		
		// 2.1 Add stream with float property
		auto[StreamId, Stream] = CreateFloatPropertyStream(*TestObject);
		FConcertReplication_ChangeStream_Request CreateStreamRequest;
		CreateStreamRequest.StreamsToAdd.Add(Stream.Pack());
		ChangeStreamForSenderClientAndValidate(TEXT("CreateStreamRequest"), CreateStreamRequest, { Stream.BaseDescription });

		// 2.2 Make a request that will fail and check that no changes were made to the original stream
		FConcertReplication_ChangeStream_Request InvalidRequest;
		FConcertPropertySelection NewSelection = GetPropertySelection(Stream, *TestObject);
		AddFloatProperty(NewSelection);
		InvalidRequest.ObjectsToPut.Add(FObjectInStreamID{ StreamId, TestObject}, FConcertReplication_ChangeStream_PutObject{ NewSelection });
		InvalidRequest.StreamsToAdd.Add(Stream.Pack()); // This will make it fail due to pre-existing stream ID

		// Server logs a warning when rejecting - avoid the test being marked with a warning.
		AddExpectedError(TEXT("Rejecting ChangeStream"));
		
		bool bReceivedChangeStreamResponse = false;
		ClientReplicationManager_Sender->ChangeStream(InvalidRequest)
			.Next([this, &bReceivedChangeStreamResponse](FConcertReplication_ChangeStream_Response&& Response)
			{
				bReceivedChangeStreamResponse = true;
				TestTrue(TEXT("ErrorCode == Handled"), Response.ErrorCode == EReplicationResponseErrorCode::Handled);
				TestTrue(TEXT("Invalid Request > Modify Stream > Failure"), Response.IsFailure());
			});
		
		TestTrue(TEXT("Invalid Request > Modify Stream > Received response"), bReceivedChangeStreamResponse);
		ValidateSenderClientStreams(TEXT("Invalid Request"), { Stream.BaseDescription });
		
		return true;
	}

	/** Removing an object from a stream over which the requester has authority, clears the authority the client had. Tests that another client can now claim authority over the previously authored properties. */
	IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FRemovingObjectClearsAuthority, FChangeStreamsTestBase, "Concert.Replication.Stream.RemovingObjectClearsAuthority", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);
	bool FRemovingObjectClearsAuthority::RunTest(const FString& Parameters)
	{
		using namespace ConcertSyncClient::Replication;

		// 1. Start clients with equivalent streams
		UTestReflectionObject* TestObject = NewObject<UTestReflectionObject>(GetTransientPackage());
		auto[SenderStreamId, SenderStream] = CreateFloatPropertyStream(*TestObject);
		auto[ReceiverStreamId, ReceiverStream] = CreateFloatPropertyStream(*TestObject);
		SenderArgs.Streams = { SenderStream };
		ReceiverArgs.Streams = { ReceiverStream };
		SetUpClientAndServer();
		
		// 2.1 Sender takes authority
		ClientReplicationManager_Sender->TakeAuthorityOver({ TestObject })
			.Next([this](FConcertReplication_ChangeAuthority_Response&& Response)
			{
				TestTrue(TEXT("ErrorCode == Handled"), Response.ErrorCode == EReplicationResponseErrorCode::Handled);
				TestTrue(TEXT("Sender > Success taking authority"), Response.RejectedObjects.IsEmpty());
			});

		// 2.2 Sender removes the object
		FConcertReplication_ChangeStream_Request RemoveObjectRequest;
		RemoveObjectRequest.ObjectsToRemove.Add({ SenderStreamId, TestObject });
		ChangeStreamForSenderClientAndValidate(TEXT("Remove Object"), RemoveObjectRequest, {});

		// 2.3 Receiver can now take authority since 2.2 implicitly removed authority
		ClientReplicationManager_Receiver->TakeAuthorityOver({ TestObject })
			.Next([this](FConcertReplication_ChangeAuthority_Response&& Response)
			{
				TestTrue(TEXT("ErrorCode == Handled"), Response.ErrorCode == EReplicationResponseErrorCode::Handled);
				TestTrue(TEXT("Receiver > Success taking authority"), Response.RejectedObjects.IsEmpty());
			});
		
		return true;
	}

	/**
     * Tests that client updates its local cache of the server state when RequestAuthorityChange times out.
     */
    IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FChangingStreamTimeoutRetainsServerState, FChangeStreamsTestBase, "Concert.Replication.Stream.ChangeStreamTimeoutRetainsServerState", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);
    bool FChangingStreamTimeoutRetainsServerState::RunTest(const FString& Parameters)
    {
    	// 1. Start up client with float property stream
    	UTestReflectionObject* TestObject = NewObject<UTestReflectionObject>(GetTransientPackage());
    	auto[InitialStreamId, InitialStream] = CreateFloatPropertyStream(*TestObject);
    	SenderArgs.Streams = { InitialStream };
    	SetUpClientAndServer();
		ServerSession->SetTestFlags(EServerSessionTestingFlags::AllowRequestTimeouts);

    	// 2. Timeout request
    	ServerSession->UnregisterCustomRequestHandler<FConcertReplication_ChangeStream_Request>();
    	FConcertReplication_ChangeStream_Request Request;
    	Request.ObjectsToRemove.Add({ InitialStreamId, TestObject });
    	ClientReplicationManager_Sender->ChangeStream(Request);

    	// 3. Check that the local client's server prediction was reverted
    	TArray<FReplicationStreamDescription> Streams = ClientReplicationManager_Sender->GetRegisteredStreams();
    	if (Streams.IsEmpty())
    	{
    		AddError(TEXT("Stream change was not reverted"));
    		return true;
    	}
    	TestTrue(TEXT("Timed out Stream change reverted"), Streams[0].BaseDescription.ReplicationMap.ReplicatedObjects.Contains(TestObject));
    	
    	return true;
    }
}
