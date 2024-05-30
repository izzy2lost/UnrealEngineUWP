// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AuthorityManager.h"
#include "ConcertMessages.h"
#include "ConcertReplicationClient.h"
#include "Enumeration/IRegistrationEnumerator.h"
#include "MuteManager.h"
#include "Replication/Formats/IObjectReplicationFormat.h"
#include "Replication/IConcertServerReplicationManager.h"
#include "Replication/Messages/Handshake.h"
#include "Replication/Misc/ReplicatedObjectHierarchyCache.h"
#include "Replication/Processing/ObjectReplicationCache.h"
#include "Replication/Processing/ServerObjectReplicationReceiver.h"
#include "SyncControlManager.h"

#include "Templates/SharedPointer.h"
#include "Templates/Tuple.h"
#include "Templates/UnrealTemplate.h"

class IConcertClientReplicationBridge;
class IConcertServerSession;

enum class EConcertSyncSessionFlags : uint32;
enum class EConcertQueryClientStreamFlags : uint8;

struct FConcertReplication_ChangeStream_Response;
struct FConcertReplication_QueryReplicationInfo_Response;
struct FConcertReplication_QueryReplicationInfo_Request;
struct FConcertAuthorityClientInfo;

namespace UE::ConcertSyncCore
{
	class IObjectReplicationFormat;
	class FObjectReplicationCache;
}

namespace UE::ConcertSyncServer::Replication
{
	class FAuthorityManager;

	/**
	 * Manages all server-side systems relevant to the Replication features.
	 * 
	 * Primarily responds to client requests to join (handshake) and leave replication delegating the result of the
	 * operation to relevant systems. 
	 */
	class FConcertServerReplicationManager
		: public IConcertServerReplicationManager
		, public IRegistrationEnumerator
		, public FNoncopyable
	{
	public:

		explicit FConcertServerReplicationManager(TSharedRef<IConcertServerSession> InLiveSession, EConcertSyncSessionFlags SessionFlags);
		virtual ~FConcertServerReplicationManager() override;

		const FAuthorityManager& GetAuthorityManager() const { return AuthorityManager; }

		//~ Begin IStreamEnumerator Interface
		virtual void ForEachStream(const FGuid& ClientEndpointId, TFunctionRef<EBreakBehavior(const FConcertReplicationStream& Stream)> Callback) const override;
		//~ End IStreamEnumerator Interface
		
		//~ Begin IClientEnumerator Interface
		virtual void ForEachReplicationClient(TFunctionRef<EBreakBehavior(const FGuid& ClientEndpointId)> Callback) const override;
		//~ End IClientEnumerator Interface

	private:
		
		/** Session instance this manager was created for. */
		TSharedRef<IConcertServerSession> Session;
		
		/** Responsible for analysing received replication data. */
		TUniquePtr<ConcertSyncCore::IObjectReplicationFormat> ReplicationFormat;

		/**
		 * Holds the outer hierarchy of all objects registered in any stream.
		 * 
		 * This receives join, leave, and change stream events.
		 * The events are processed after the mute manager does.
		 */
		ConcertSyncCore::FReplicatedObjectHierarchyCache ServerObjectCache;

		/** Responds to client requests to changing authority and can be asked whether an object change is valid to take place. */
		FAuthorityManager AuthorityManager;
		/** Responds to client mute requests and stores the mute states. */
		FMuteManager MuteManager;
		/** Decides whether clients should be replicating. Clients may replicate when they have authority and there are other clients listening for that data. */
		FSyncControlManager SyncControlManager;
		
		/** Received replication events are put into the ReplicationCache. The cache is used to relay data to clients latently. */
		const TSharedRef<ConcertSyncCore::FObjectReplicationCache> ReplicationCache;
		/** Receives replication events from all endpoints. */
		FServerObjectReplicationReceiver ReplicationDataReceiver;

		/**
		 * Clients that have requested to join replication. Maps client ID to replication info.
		 * Clients are stored in a unique ptr to avoid dealing with reallocations when the map resizes.
		 */
		TMap<FGuid, TUniquePtr<FConcertReplicationClient>> Clients;

		// Joining
		EConcertSessionResponseCode HandleJoinReplicationSessionRequest(const FConcertSessionContext& ConcertSessionContext, const FConcertReplication_Join_Request& Request, FConcertReplication_Join_Response& Response);
		EConcertSessionResponseCode InternalHandleJoinReplicationSessionRequest(const FConcertSessionContext& ConcertSessionContext, const FConcertReplication_Join_Request& Request, FConcertReplication_Join_Response& Response);

		// Querying
		EConcertSessionResponseCode HandleQueryReplicationInfoRequest(const FConcertSessionContext& ConcertSessionContext, const FConcertReplication_QueryReplicationInfo_Request& Request, FConcertReplication_QueryReplicationInfo_Response& Response);
		/** Gets all registered streams and optionally removes the properties. */
		static TArray<FConcertBaseStreamInfo> BuildClientStreamInfo(const FConcertReplicationClient& Client, EConcertQueryClientStreamFlags QueryFlags);
		/** Maps the client's streams to the objects in that stream the client has taken authority over. */
		TArray<FConcertAuthorityClientInfo> BuildClientAuthorityInfo(const FConcertReplicationClient& Client) const;

		// Changing streams
		EConcertSessionResponseCode HandleChangeStreamRequest(const FConcertSessionContext& ConcertSessionContext, const FConcertReplication_ChangeStream_Request& Request, FConcertReplication_ChangeStream_Response& Response);

		// Leaving
		void HandleLeaveReplicationSessionRequest(const FConcertSessionContext& ConcertSessionContext, const FConcertReplication_LeaveEvent& EventData);
		void OnConnectionChanged(IConcertServerSession& ConcertServerSession, EConcertClientStatus ConcertClientStatus, const FConcertSessionClientInfo& ConcertSessionClientInfo);

		void OnClientLeftReplication(const FGuid& EndpointId);
		
		/**
		 * Ticks all clients which causes clients to process pending data and send it to the corresponding endpoints.
		 * 
		 * There is an internal time budget to ensure ticks do not starve the server's other tick tasks.
		 * This is configured in an .ini. TODO: Add config
		 */
		void Tick(IConcertServerSession& InSession, float InDeltaTime);
		
		/** Callback to clients for obtaining an object's frequency settings. */
		FConcertObjectReplicationSettings GetObjectFrequencySettings(const FConcertReplicatedObjectId& Object) const;
	};
}