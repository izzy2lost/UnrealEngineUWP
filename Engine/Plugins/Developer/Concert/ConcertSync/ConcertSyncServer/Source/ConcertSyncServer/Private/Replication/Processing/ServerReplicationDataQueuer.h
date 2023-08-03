// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Set.h"
#include "Misc/Guid.h"
#include "Replication/Processing/ReplicationDataQueuer.h"
#include "Templates/SharedPointer.h"

namespace UE::ConcertSyncServer::Replication
{
	class FConcertReplicationClient;
	
	/**
	 * Queues events for a specific client.
	 * TODO: Filters events by matching client and stream attributes.
	 */
	class FServerReplicationDataQueuer : public ConcertSyncCore::FReplicationDataQueuer
	{
		template <typename ObjectType, ESPMode Mode>
		friend class SharedPointerInternals::TIntrusiveReferenceController;
	public:
		
		static TSharedRef<FServerReplicationDataQueuer> Make(const FConcertReplicationClient& OwningClient, TSharedRef<ConcertSyncCore::FObjectReplicationCache> InReplicationCache);
		
		//~ Begin IReplicationCacheUser Interface
		virtual bool WantsToAcceptObject(const ConcertSyncCore::FReplicationStreamObjectID& Object) const override;
		//~ End IReplicationCacheUser Interface

	private:

		FServerReplicationDataQueuer(const FConcertReplicationClient& OwningClient);

		/** The client for which this FServerReplicationDataQueuer exists. */
		const TSet<FGuid> OwningClientStreamIds;
	};
}

