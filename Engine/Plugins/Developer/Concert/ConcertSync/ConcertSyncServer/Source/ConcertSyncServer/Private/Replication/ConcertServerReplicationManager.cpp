// Copyright Epic Games, Inc. All Rights Reserved.

#include "ConcertServerReplicationManager.h"

#include "AuthorityManager.h"
#include "ConcertLogGlobal.h"
#include "IConcertSession.h"
#include "Replication/ConcertReplicationClient.h"
#include "Replication/Data/ClientQueriedInfo.h"
#include "Replication/Formats/FullObjectFormat.h"
#include "Replication/Messages/ChangeStream.h"
#include "Replication/Messages/ClientQuery.h"
#include "Replication/Messages/Handshake.h"
#include "Replication/Processing/ObjectReplicationCache.h"
#include "Util/JoinRequestValidation.h"

namespace UE::ConcertSyncServer::Replication
{
	FConcertServerReplicationManager::FConcertServerReplicationManager(TSharedRef<IConcertServerSession> InLiveSession)
		: Session(MoveTemp(InLiveSession))
		, ReplicationFormat(MakeShared<ConcertSyncCore::FFullObjectFormat>())
		, AuthorityManager(MakeShared<FAuthorityManager>(*this, Session))
		, ReplicationCache(MakeShared<ConcertSyncCore::FObjectReplicationCache>(ReplicationFormat))
		, ReplicationDataReceiver(AuthorityManager, Session, ReplicationCache)
	{
		Session->RegisterCustomRequestHandler<FConcertReplication_Join_Request, FConcertReplication_Join_Response>(this, &FConcertServerReplicationManager::HandleJoinReplicationSessionRequest);
		Session->RegisterCustomRequestHandler<FConcertReplication_QueryReplicationInfo_Request, FConcertReplication_QueryReplicationInfo_Response>(this, &FConcertServerReplicationManager::HandleQueryReplicationInfoRequest);
		Session->RegisterCustomRequestHandler<FConcertReplication_ChangeStream_Request, FConcertReplication_ChangeStream_Response>(this, &FConcertServerReplicationManager::HandleChangeStreamRequest);
		Session->RegisterCustomEventHandler<FConcertReplication_LeaveEvent>(this, &FConcertServerReplicationManager::HandleLeaveReplicationSessionRequest);
		Session->OnSessionClientChanged().AddRaw(this, &FConcertServerReplicationManager::OnConnectionChanged);

		Session->OnTick().AddRaw(this, &FConcertServerReplicationManager::Tick);
	}

	FConcertServerReplicationManager::~FConcertServerReplicationManager()
	{
		Session->UnregisterCustomRequestHandler<FConcertReplication_Join_Response>();
		Session->UnregisterCustomRequestHandler<FConcertReplication_QueryReplicationInfo_Response>();
		Session->UnregisterCustomEventHandler<FConcertReplication_LeaveEvent>(this);

		Session->OnTick().RemoveAll(this);
	}

	void FConcertServerReplicationManager::ForEachStream(const FGuid& ClientEndpointId, TFunctionRef<EBreakBehavior(const FReplicationStreamDescription& Stream)> Callback) const
	{
		const TSharedRef<FConcertReplicationClient>* Client = Clients.Find(ClientEndpointId);
		if (!ensure(Client))
		{
			return;
		}

		for (const FReplicationStreamDescription& Stream : (*Client)->GetStreamDescriptions())
		{
			if (Callback(Stream) == EBreakBehavior::Break)
			{
				break;
			}
		}
	}

	void FConcertServerReplicationManager::ForEachSendingClient(TFunctionRef<EBreakBehavior(const FGuid& ClientEndpointId)> Callback) const
	{
		for (const TPair<FGuid, TSharedRef<FConcertReplicationClient>>& ClientPair : Clients)
		{
			if (!ClientPair.Value->GetStreamDescriptions().IsEmpty()
				&& Callback(ClientPair.Key) == EBreakBehavior::Break)
			{
				break;
			}
		}
	}

	EConcertSessionResponseCode FConcertServerReplicationManager::HandleJoinReplicationSessionRequest(
		const FConcertSessionContext& ConcertSessionContext,
		const FConcertReplication_Join_Request& Request,
		FConcertReplication_Join_Response& Response
		)
	{
		// Have a pair of logs before and after processing in case of potential disaster
		UE_LOG(LogConcert, Log, TEXT("Received replication join request from endpoint %s"), *ConcertSessionContext.SourceEndpointId.ToString());
		
		const EConcertSessionResponseCode Result = InternalHandleJoinReplicationSessionRequest(ConcertSessionContext, Request, Response);
		
		UE_CLOG(Response.JoinErrorCode == EJoinReplicationErrorCode::Success, LogConcert, Log, TEXT("Accepted replication join request"));
		UE_CLOG(Response.JoinErrorCode != EJoinReplicationErrorCode::Success, LogConcert, Log, TEXT("Rejected replication join request. %s: %s"), *ConcertSyncCore::Replication::LexJoinErrorCode(Response.JoinErrorCode), *Response.DetailedErrorMessage);
		return Result;
	}

	EConcertSessionResponseCode FConcertServerReplicationManager::InternalHandleJoinReplicationSessionRequest(
		const FConcertSessionContext& ConcertSessionContext,
		const FConcertReplication_Join_Request& Request,
		FConcertReplication_Join_Response& Response
		)
	{
		const FGuid& ClientId = ConcertSessionContext.SourceEndpointId;
		const bool bHasClient = Session->GetSessionClientEndpointIds().Contains(ClientId);
		if (!bHasClient)
		{
			Response = { EJoinReplicationErrorCode::NotInAnyConcertSession, TEXT("Client must be in a Concert Session!") };
			return EConcertSessionResponseCode::Success;
		}

		if (Clients.Contains(ClientId))
		{
			Response = { EJoinReplicationErrorCode::AlreadyInSession };
			return EConcertSessionResponseCode::Success;
		}

		auto[ErrorCode, ErrorMessage, StreamDescriptions] = ValidateRequest(Request);
		if (ErrorCode != EJoinReplicationErrorCode::Success)
		{
			Response = { ErrorCode, ErrorMessage };
			return EConcertSessionResponseCode::Success;
		}

		Clients.Emplace(ClientId, MakeShared<FConcertReplicationClient>(MoveTemp(StreamDescriptions), ClientId, Session, ReplicationCache));
		Response = { EJoinReplicationErrorCode::Success };
		return EConcertSessionResponseCode::Success;
	}

	EConcertSessionResponseCode FConcertServerReplicationManager::HandleQueryReplicationInfoRequest(
		const FConcertSessionContext& ConcertSessionContext,
		const FConcertReplication_QueryReplicationInfo_Request& Request,
		FConcertReplication_QueryReplicationInfo_Response& Response
		)
	{
		for (const FGuid& EndpointId : Request.ClientEndpointIds)
		{
			const TSharedRef<FConcertReplicationClient>* Client = Clients.Find(EndpointId);
			if (!Client)
			{
				// This could happen if the client left the replication session before this request was answered
				continue;
			}
			
			FReplicationClientQueriedInfo& EndpointInfo = Response.ClientInfo.Add(EndpointId);
			if (!EnumHasAnyFlags(Request.QueryFlags, EConcertQueryClientStreamFlags::SkipStreamInfo))
			{
				const bool bSkipProperties = EnumHasAnyFlags(Request.QueryFlags, EConcertQueryClientStreamFlags::SkipProperties);
				EndpointInfo.Streams = BuildClientStreamInfo(Client->Get(), bSkipProperties);
			}
			if (!EnumHasAnyFlags(Request.QueryFlags, EConcertQueryClientStreamFlags::SkipAuthority))
			{
				EndpointInfo.Authority = BuildClientAuthorityInfo(Client->Get());
			}
		}

		Response.ErrorCode = EReplicationResponseErrorCode::Handled;
		return EConcertSessionResponseCode::Success;
	}

	TArray<FSharedReplicationStreamDescription> FConcertServerReplicationManager::BuildClientStreamInfo(const FConcertReplicationClient& Client, bool bSkipProperties) const
	{
		TArray<FSharedReplicationStreamDescription> Result;
		Algo::Transform(Client.GetStreamDescriptions(), Result, [bSkipProperties](const FReplicationStreamDescription& Description)
		{
			if (!bSkipProperties)
			{
				return Description.BaseDescription;
			}

			const FSharedReplicationStreamDescription& CopiedDescription = Description.BaseDescription;
			const FObjectReplicationMap& CopiedReplicationMap = CopiedDescription.ReplicationMap;

			// It was requested to skip sending the properties (saves network bandwidth)
			FSharedReplicationStreamDescription StrippedResult;
			StrippedResult.Identifier = CopiedDescription.Identifier;
			StrippedResult.ReplicationMap.ReplicatedObjects.Reserve(CopiedReplicationMap.ReplicatedObjects.Num());
			for (const TPair<FSoftObjectPath, FReplicatedObjectInfo>& ObjectInfo : CopiedReplicationMap.ReplicatedObjects)
			{
				StrippedResult.ReplicationMap.ReplicatedObjects.Add(ObjectInfo.Key, { ObjectInfo.Value.ClassPath });
			}
			return StrippedResult;
		});

		return Result;
	}

	TArray<FReplicationAuthorityInfo> FConcertServerReplicationManager::BuildClientAuthorityInfo(const FConcertReplicationClient& Client) const
	{
		TArray<FReplicationAuthorityInfo> Result;
		for (const FReplicationStreamDescription& Description : Client.GetStreamDescriptions())
		{
			const FGuid StreamId = Description.BaseDescription.Identifier;
			FReplicationAuthorityInfo Info;
			Info.StreamId = StreamId;
			
			for (const TPair<FSoftObjectPath, FReplicatedObjectInfo>& Pair : Description.BaseDescription.ReplicationMap.ReplicatedObjects)
			{
				FReplicatedObjectId ObjectInfo;
				ObjectInfo.SenderEndpointId = Client.GetClientEndpointId();
				ObjectInfo.Object = Pair.Key;
				ObjectInfo.StreamId = StreamId;
				
				if (AuthorityManager->HasAuthorityToChange(ObjectInfo))
				{
					Info.AuthoredObjects.Add(Pair.Key);
				}
			}

			// There is no point in sending empty data
			if (!Info.AuthoredObjects.IsEmpty())
			{
				Result.Add(Info);
			}
		}
		return Result;
	}

	void FConcertServerReplicationManager::HandleLeaveReplicationSessionRequest(
		const FConcertSessionContext& ConcertSessionContext,
		const FConcertReplication_LeaveEvent& EventData)
	{
		const FGuid ClientEndpointId = ConcertSessionContext.SourceEndpointId;
		UE_LOG(LogConcert, Log, TEXT("Received replication leave request from endpoint %s"), *ClientEndpointId.ToString());
		
		Clients.Remove(ClientEndpointId);
		AuthorityManager->OnClientLeft(ClientEndpointId);
	}

	void FConcertServerReplicationManager::OnConnectionChanged(IConcertServerSession& ConcertServerSession, EConcertClientStatus ConcertClientStatus, const FConcertSessionClientInfo& ClientInfo)
	{
		const FGuid ClientEndpointId = ClientInfo.ClientEndpointId;
		if (ConcertClientStatus == EConcertClientStatus::Disconnected)
		{
			Clients.Remove(ClientEndpointId);
			AuthorityManager->OnClientLeft(ClientEndpointId);
		}
	}

	void FConcertServerReplicationManager::Tick(IConcertServerSession& InSession, float InDeltaTime)
	{
		// TODO: Time slice replication clients
		for (TPair<FGuid, TSharedRef<FConcertReplicationClient>>& Client : Clients)
		{
			// TODO: Load time budget from config
			constexpr float TimeBudget = 1.f / 60.f;
			Client.Value->ProcessClient(TimeBudget);
		}
	}
}

