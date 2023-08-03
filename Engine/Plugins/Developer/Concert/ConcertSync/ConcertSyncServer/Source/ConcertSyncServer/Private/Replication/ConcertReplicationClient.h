// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Replication/Processing/ObjectReplicationSender.h"
#include "Templates/SharedPointer.h"

struct FReplicationStreamDescription;
struct FConcertReplication_Join_Request;

namespace UE::ConcertSyncCore
{
	class FObjectReplicationCache;
}

namespace UE::ConcertSyncServer::Replication
{
	class FServerReplicationDataQueuer;

	/** Server-side representation of a remote replication client. */
	class FConcertReplicationClient : public FNoncopyable
	{
	public:

		FConcertReplicationClient(
			TArray<FReplicationStreamDescription> StreamDescriptions,
			const FGuid& ClientEndpointId,
			TSharedRef<IConcertSession> Session,
			TSharedRef<ConcertSyncCore::FObjectReplicationCache> ReplicationCache
			);

		/**
		 * Process any latent tasks, such as processing pending events that need to be sent to the remote instance.
		 * Process given a time budget. The time budget may be exceeded but we'll try not to and to stay as close to the budget as possible.
		 */
		void ProcessClient(float TimeBudget);

		const FGuid& GetClientEndpointId() const { return ClientEndpointId; }

		const TArray<FReplicationStreamDescription>& GetStreamDescriptions() const { return StreamDescriptions; }

	private:

		/** The streams this client offered to send. */
		const TArray<FReplicationStreamDescription> StreamDescriptions;

		/** This client's endpoint ID. */
		const FGuid ClientEndpointId;
		
		/** Queues up replication data and passes it to DataRelay. */
		TSharedRef<FServerReplicationDataQueuer> EventQueue;

		/** Sends data to the remote endpoint */
		ConcertSyncCore::FObjectReplicationSender DataRelay;
	};
}
