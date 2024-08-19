// Copyright Epic Games, Inc. All Rights Reserved.

#include "RejoinStreamAndAuthorityPredictor.h"

#include "EndpointCache.h"
#include "Replication/Messages/ReplicationActivity.h"

#include "Replication/Misc/ClientPredictionUtils.h"

namespace UE::MultiUserClient::Replication
{
	namespace Private
	{
		static void ExtractMultiUserContent(
			const TArray<FConcertBaseStreamInfo>& Streams,
			const TArray<FConcertObjectInStreamID>& Authority,
			FConcertBaseStreamInfo& OutPredictedStream,
			TSet<FSoftObjectPath>& OutPredictedAuthority
			)
		{
			// A user may have created their custom streams, so find the MultiUserStreamID one.
			const FConcertBaseStreamInfo* MultiUserStream = Streams.FindByPredicate([](const FConcertBaseStreamInfo& Stream)
			{
				return Stream.Identifier == MultiUserStreamID;
			});
			if (!MultiUserStream)
			{
				return;
			}
			
			OutPredictedStream = *MultiUserStream;
			// Again... a user may have created their custom streams, so only return the authority of the MultiUserStreamID one.
			Algo::TransformIf(Authority, OutPredictedAuthority,
				[](const FConcertObjectInStreamID& ObjectInStreamID) { return ObjectInStreamID.StreamId == MultiUserStreamID; },
				[](const FConcertObjectInStreamID& ObjectInStreamID) { return ObjectInStreamID.Object; }
			);
		}
		
		static void AnalyzeActivityHistory(
			const IConcertClientWorkspace& Workspace,
			const FConcertClientInfo& ClientInfo,
			FConcertBaseStreamInfo& OutPredictedStream,
			TSet<FSoftObjectPath>& OutPredictedAuthority
			)
		{
			TArray<FConcertBaseStreamInfo> Streams;
			TArray<FConcertObjectInStreamID> Authority;
			const TOptional<int64> FoundActivity = ConcertSyncClient::Replication::IncrementalBacktrackActivityHistoryForActivityThatSetsContent(
				Workspace, ClientInfo, Streams, Authority
				);
			if (FoundActivity.IsSet())
			{
				ExtractMultiUserContent(Streams, Authority, OutPredictedStream, OutPredictedAuthority);
			}
		}
	}
	
	FRejoinStreamAndAuthorityPredictor::FRejoinStreamAndAuthorityPredictor(const IConcertClientWorkspace& InWorkspace, FConcertClientInfo InClientInfo)
		: ClientInfo(MoveTemp(InClientInfo))
		, PredictedStream({ .Identifier = MultiUserStreamID })
	{
		// For now, we only look at the FConcertSyncReplicationPayload_LeaveReplication activity. It can be looked up once.
		// In the future, we'll add an event for FConcertReplication_PutState_Requests, which can affect offline clients, invoked by presets.
		// Then, we'll have to listen for activity changes.
		Private::AnalyzeActivityHistory(InWorkspace, ClientInfo, PredictedStream, PredictedAuthority);
	}
}
