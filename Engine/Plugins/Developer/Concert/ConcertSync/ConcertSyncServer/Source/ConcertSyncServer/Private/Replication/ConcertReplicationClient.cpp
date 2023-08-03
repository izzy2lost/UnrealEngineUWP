// Copyright Epic Games, Inc. All Rights Reserved.

#include "ConcertReplicationClient.h"

#include "Processing/ServerReplicationDataQueuer.h"
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
		, EventQueue(FServerReplicationDataQueuer::Make(*this, MoveTemp(ReplicationCache)))
		, DataRelay(ClientEndpointId, MoveTemp(Session), EventQueue)
	{}

	void FConcertReplicationClient::ProcessClient(float TimeBudget)
	{
		DataRelay.ProcessObjects(TimeBudget);
	}
}
