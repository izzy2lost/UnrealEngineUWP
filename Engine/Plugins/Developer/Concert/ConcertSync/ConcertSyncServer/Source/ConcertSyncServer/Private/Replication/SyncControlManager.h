// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ConcertMessageData.h"
#include "Replication/Data/ObjectIds.h"

#include "HAL/Platform.h"
#include "Replication/Messages/SyncControl.h"
#include "Templates/UnrealTemplate.h"

class IConcertServerSession;
enum class EConcertClientStatus : uint8;
struct FConcertSessionClientInfo;

namespace UE::ConcertSyncServer::Replication
{
	class IRegistrationEnumerator;
	class FAuthorityManager;
	
	/**
	 * Decides whether clients should be replicating.
	 * Clients may replicate when they have authority and there are other clients listening for that data.
	 *
	 * For now, sync control just checks whether there is another client in the session.
	 * TODO: In the future, we should consider whether there is another client in the same world, as well.
	 */
	class FSyncControlManager : public FNoncopyable
	{
	public:
		
		FSyncControlManager(
			IConcertServerSession& Session UE_LIFETIMEBOUND,
			FAuthorityManager& AuthorityManager UE_LIFETIMEBOUND,
			const IRegistrationEnumerator& Getters UE_LIFETIMEBOUND
			);
		~FSyncControlManager();

		/** @return Whether this Object is allowed to processed. */
		bool HasSyncControl(const FConcertReplicatedObjectId& Object) const;

		/** Called by FConcertServerReplicationManager when client completes replication handshake. */
		FConcertReplication_ChangeSyncControl OnGenerateSyncControlForClientJoin(const FGuid& ClientId);
		
		/** Called by FConcertServerReplicationManager when client completes leaves replication. */
		void OnClientLeft(const FGuid& ClientId) { HandleClientLeave(ClientId); }

	private:

		/** Used to send sync control messages to clients, which notifies them to start / stop replicating. */
		IConcertServerSession& Session;
		/** Used to detect whether a client has authority. */
		FAuthorityManager& AuthorityManager;

		/** Callbacks for retrieving more info about client replication registration. */
		const IRegistrationEnumerator& Getters;

		struct FClientData
		{
			/** Contains all objects that are allowed to be replicated for this client. */
			TSet<FConcertObjectInStreamID> ObjectsWithSyncControl;
		};
		/** Maps client ID to client sync control data. */
		TMap<FGuid, FClientData> PerClientData;

		/** Generates new sync control for client that is explicitly changing their authority. */
		FConcertReplication_ChangeSyncControl OnGenerateSyncControlForAuthorityResponse(const FGuid& ClientId);

		/** Cleans up the associated client data and updates sync control for all other clients. */
		void HandleClientLeave(const FGuid& LeftClientId);

		/** Compares the client's current sync control against the new sync control it should have and returns the delta to be sent to the client. */
		FConcertReplication_ChangeSyncControl RefreshClientSyncControl(
			const FGuid& ClientId, 
			TFunctionRef<bool(const FConcertObjectInStreamID& Object, bool bNewValue)> ShouldSkipInMessage = [](auto&, auto){ return false; }
			);

		/** Except for SkippedClient, checks all clients' sync control has changed and conditionally updates the remote endpoints. */
		void RefreshAndSendToAllClientsExcept(const FGuid& SkippedClient);
		/** Checks whether client sync control has changed and conditionally updates the remote endpoint. */
		void RefreshAndSendSyncControl(const FGuid& ClientId);

		/** @return Whether any client wants to receive Object. */
		bool IsAnyoneInterestedIn(const FConcertReplicatedObjectId& Object) const;
		/** @return Whether Client is interested in receiving Object. */
		bool IsClientInterestedIn(const FConcertReplicatedObjectId& Object, const FGuid& Client) const;
	};
}

