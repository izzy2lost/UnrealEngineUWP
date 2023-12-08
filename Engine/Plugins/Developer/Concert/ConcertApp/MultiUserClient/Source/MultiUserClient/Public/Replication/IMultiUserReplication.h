// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"

struct FGuid;
struct FObjectReplicationMap;

namespace UE::MultiUserClient
{
	class IReplicationDiscoverer;
	
	/** Interface for interacting with Multi-User replication, which uses the Concert replication system. */
	class MULTIUSERCLIENT_API IMultiUserReplication
	{
	public:

		/**
		 * The server keeps track of multiple streams per client and the ID is used to identify streams by client.
		 * MU only uses one stream per client and this is its ID.
		 * 
		 * @return The stream ID that every MU client uses to register its replication stream with the server.
		 */
		virtual FGuid GetMultiUserStreamId() const = 0;

		/**
		 * @return Gets the last known server map of objects registered for replication for a given client.
		 * This server state is regularly polled whilst the local client state should always be in synch.*/
		virtual const FObjectReplicationMap* FindReplicationMapForClient(const FGuid& ClientId) const = 0;
		/** @return Whether the local editor instance thinks the client has authority over the properties it has registered to ObjectPath. */
		virtual bool IsReplicatingObject(const FGuid& ClientId, const FSoftObjectPath& ObjectPath) const = 0;

		/**
		 * Register a discoverer.
		 *
		 * It is used to automatically configure UObjects for replication when appropriate:
		 * - When an user adds an object via Add Actor button
		 * - When an UObject is added to the world via a transaction (run on the client machine that adds the UObject)
		 */
		virtual void RegisterReplicationDiscoverer(TSharedRef<IReplicationDiscoverer> Discoverer) = 0;
		/** Unregisters a previously registered discoverer */
		virtual void RemoveReplicationDiscoverer(const TSharedRef<IReplicationDiscoverer>& Discoverer) = 0;

		virtual ~IMultiUserReplication() = default;
	};
}

