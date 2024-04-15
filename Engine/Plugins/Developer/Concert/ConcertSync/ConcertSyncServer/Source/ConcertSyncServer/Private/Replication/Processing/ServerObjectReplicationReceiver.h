// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Replication/Processing/ObjectReplicationReceiver.h"

namespace UE::ConcertSyncServer::Replication
{
	class FAuthorityManager;
	
	/** Rejects changes to objects that the sending client does not have authority over. */
	class FServerObjectReplicationReceiver : public ConcertSyncCore::FObjectReplicationReceiver
	{
	public:

		FServerObjectReplicationReceiver(
			const FAuthorityManager& AuthorityManager UE_LIFETIMEBOUND,
			IConcertSession& Session UE_LIFETIMEBOUND,
			ConcertSyncCore::FObjectReplicationCache& ReplicationCache UE_LIFETIMEBOUND
			);

	protected:

		//~ Begin FObjectReplicationReceiver Interface
		virtual bool ShouldAcceptObject(const FConcertSessionContext& SessionContext, const FConcertReplication_StreamReplicationEvent& StreamEvent, const FConcertReplication_ObjectReplicationEvent& ObjectEvent) const override;
		//~ End FObjectReplicationReceiver Interface

	private:

		/** Used to determine whether a client has authority over objects. */
		const FAuthorityManager& AuthorityManager;
	};
}


