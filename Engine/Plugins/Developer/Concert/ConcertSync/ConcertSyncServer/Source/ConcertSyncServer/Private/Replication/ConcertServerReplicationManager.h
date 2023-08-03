// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ConcertMessages.h"
#include "ConcertReplicationClient.h"
#include "IConcertSessionHandler.h"
#include "Replication/IConcertServerReplicationManager.h"
#include "Replication/Messages/ConcertReplicationHandshakeMessages.h"
#include "Replication/Processing/ObjectReplicationReceiver.h"
#include "Templates/SharedPointer.h"
#include "Templates/Tuple.h"

namespace UE::ConcertSyncCore
{
	class IObjectReplicationFormat;
	class FObjectReplicationCache;
}

class IConcertClientReplicationBridge;
class IConcertServerSession;

namespace UE::ConcertSyncServer::Replication
{
	class FConcertServerReplicationManager : public IConcertServerReplicationManager
	{
	public:
		explicit FConcertServerReplicationManager(TSharedRef<IConcertServerSession> InLiveSession);
		virtual ~FConcertServerReplicationManager() override;

	private:
		
		/** Session instance this manager was created for. */
		TSharedRef<IConcertServerSession> Session;
		
		/** Responsible for analysing received replication data. */
		TSharedRef<ConcertSyncCore::IObjectReplicationFormat> ReplicationFormat;
		
		/** Received replication events are put into the ReplicationCache. The cache is used to relay data to clients latently. */
		TSharedRef<ConcertSyncCore::FObjectReplicationCache> ReplicationCache;
		/** Receives replication events from all endpoints. */
		ConcertSyncCore::FObjectReplicationReceiver ReplicationDataReceiver;

		/** Clients that have requested to join replication. Maps client ID to replication info. */
		TMap<FGuid, TSharedRef<FConcertReplicationClient>> Clients;

		// Event handlers
		EConcertSessionResponseCode HandleJoinReplicationSessionRequest(const FConcertSessionContext& ConcertSessionContext, const FConcertReplication_Join_Request& Request, FConcertReplication_Join_Response& Response);
		EConcertSessionResponseCode InternalHandleJoinReplicationSessionRequest(const FConcertSessionContext& ConcertSessionContext, const FConcertReplication_Join_Request& Request, FConcertReplication_Join_Response& Response);
		void HandleLeaveReplicationSessionRequest(const FConcertSessionContext& ConcertSessionContext, const FConcertReplication_LeaveEvent& EventData);
		
		void OnConnectionChanged(IConcertServerSession& ConcertServerSession, EConcertClientStatus ConcertClientStatus, const FConcertSessionClientInfo& ConcertSessionClientInfo);

		/**
		 * Ticks all clients which causes clients to process pending data and send it to the corresponding endpoints.
		 * 
		 * There is an internal time budget to ensure ticks do not starve the server's other tick tasks.
		 * This is configured in an .ini. TODO: Add config
		 */
		void Tick(IConcertServerSession& InSession, float InDeltaTime);
	};
}