// Copyright Epic Games, Inc. All Rights Reserved.

#include "ServerObjectReplicationReceiver.h"

#include "IConcertSessionHandler.h"
#include "Replication/AuthorityManager.h"
#include "Replication/ReplicationStreamObjectID.h"
#include "Replication/Messages/ConcertReplicationEvents.h"

namespace UE::ConcertSyncServer::Replication
{
	FServerObjectReplicationReceiver::FServerObjectReplicationReceiver(
		TSharedRef<FAuthorityManager> AuthorityManager,
		TSharedRef<IConcertSession> Session,
		TSharedRef<ConcertSyncCore::FObjectReplicationCache> ReplicationCache
		)
		: FObjectReplicationReceiver(MoveTemp(Session), MoveTemp(ReplicationCache))
		, AuthorityManager(MoveTemp(AuthorityManager))
	{}

	bool FServerObjectReplicationReceiver::ShouldAcceptObject(
		const FConcertSessionContext& SessionContext,
		const FConcertStreamReplicationEvent& StreamEvent,
		const FConcertObjectReplicationEvent& ObjectEvent
		) const
	{
		const ConcertSyncCore::FReplicatedObjectInfo ReplicatedObjectInfo { { StreamEvent.StreamId, ObjectEvent.ReplicatedObject }, SessionContext.SourceEndpointId };
		return AuthorityManager->IsObjectChangeAllowed(ReplicatedObjectInfo);
	}
}
