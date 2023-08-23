// Copyright Epic Games, Inc. All Rights Reserved.

#include "ConcertServerReplicationManager.h"

#include "AuthorityManager.h"
#include "ConcertLogGlobal.h"
#include "IConcertSession.h"
#include "Replication/ConcertReplicationClient.h"
#include "Replication/Formats/FullObjectFormat.h"
#include "Replication/Messages/ConcertReplicationHandshakeMessages.h"
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
		Session->RegisterCustomEventHandler<FConcertReplication_LeaveEvent>(this, &FConcertServerReplicationManager::HandleLeaveReplicationSessionRequest);
		Session->OnSessionClientChanged().AddRaw(this, &FConcertServerReplicationManager::OnConnectionChanged);

		Session->OnTick().AddRaw(this, &FConcertServerReplicationManager::Tick);
	}

	FConcertServerReplicationManager::~FConcertServerReplicationManager()
	{
		Session->UnregisterCustomRequestHandler<FConcertReplication_Join_Response>();
		Session->UnregisterCustomEventHandler<FConcertReplication_LeaveEvent>(this);

		Session->OnTick().RemoveAll(this);
	}

	void FConcertServerReplicationManager::ForEachStream(const FGuid& ClientEndpointId, TFunctionRef<EBreakBehavior(const FReplicationStreamDescription& Stream)> Callback)
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

	void FConcertServerReplicationManager::ForEachSendingClient(TFunctionRef<EBreakBehavior(const FGuid& ClientEndpointId)> Callback)
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
		
		UE_CLOG(Response.ErrorCode == EJoinReplicationErrorCode::Success, LogConcert, Log, TEXT("Accepted replication join request"));
		UE_CLOG(Response.ErrorCode != EJoinReplicationErrorCode::Success, LogConcert, Log, TEXT("Rejected replication join request. %s: %s"), *ConcertSyncCore::Replication::LexJoinErrorCode(Response.ErrorCode), *Response.DetailedErrorMessage);
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

	void FConcertServerReplicationManager::HandleLeaveReplicationSessionRequest(
		const FConcertSessionContext& ConcertSessionContext,
		const FConcertReplication_LeaveEvent& EventData
		)
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

