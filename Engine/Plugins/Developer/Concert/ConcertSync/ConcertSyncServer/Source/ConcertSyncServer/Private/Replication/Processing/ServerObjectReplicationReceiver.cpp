// Copyright Epic Games, Inc. All Rights Reserved.

#include "ServerObjectReplicationReceiver.h"

#include "IConcertSessionHandler.h"
#include "Replication/AuthorityManager.h"
#include "Replication/Data/ObjectIds.h"
#include "Replication/Messages/ObjectReplication.h"

namespace UE::ConcertSyncServer::Replication
{
	FServerObjectReplicationReceiver::FServerObjectReplicationReceiver(
		const FAuthorityManager& AuthorityManager,
		IConcertSession& Session,
		ConcertSyncCore::FObjectReplicationCache& ReplicationCache
		)
		: FObjectReplicationReceiver(Session, ReplicationCache)
		, AuthorityManager(AuthorityManager)
	{}

	bool FServerObjectReplicationReceiver::ShouldAcceptObject(
		const FConcertSessionContext& SessionContext,
		const FConcertReplication_StreamReplicationEvent& StreamEvent,
		const FConcertReplication_ObjectReplicationEvent& ObjectEvent
		) const
	{
		const FConcertReplicatedObjectId ReplicatedObjectInfo { { StreamEvent.StreamId, ObjectEvent.ReplicatedObject }, SessionContext.SourceEndpointId };
		return AuthorityManager.HasAuthorityToChange(ReplicatedObjectInfo);
	}
}
