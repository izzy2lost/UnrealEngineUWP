// Copyright Epic Games, Inc. All Rights Reserved.

#include "ConcertReplicationClient.h"

#include "Processing/ServerReplicationDataQueuer.h"
#include "Replication/ChangeStreamSharedUtils.h"
#include "Replication/Messages/ConcertReplicationHandshakeMessages.h"

namespace UE::ConcertSyncServer::Replication
{
	FConcertReplicationClient::FConcertReplicationClient(
		TArray<FReplicationStreamDescription> StreamDescriptions,
		const FGuid& ClientEndpointId,
		TSharedRef<IConcertSession> Session,
		TSharedRef<ConcertSyncCore::FObjectReplicationCache> ReplicationCache
	)
		: StreamDescriptions(MoveTemp(StreamDescriptions))
		, ClientEndpointId(ClientEndpointId)
		, EventQueue(FServerReplicationDataQueuer::Make(ClientEndpointId, MoveTemp(ReplicationCache)))
		, DataRelay(ClientEndpointId, MoveTemp(Session), EventQueue)
	{}

	void FConcertReplicationClient::ProcessClient(float TimeBudget)
	{
		DataRelay.ProcessObjects(TimeBudget);
	}

	void FConcertReplicationClient::ApplyValidatedRequest(const FConcertChangeStream_Request& Request)
	{
		// Right now there is nothing further to do but if in future you need to update some client systems of the change, this is the place to do it.
		ConcertSyncCore::Replication::ChangeStreamUtils::ApplyValidatedRequest(Request, StreamDescriptions);
	}
}
