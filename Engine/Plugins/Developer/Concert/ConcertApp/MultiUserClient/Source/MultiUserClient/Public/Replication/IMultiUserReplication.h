// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/Attribute.h"
#include "Templates/SharedPointer.h"

struct FGuid;
struct FObjectReplicationMap;

namespace UE::MultiUserClient
{
	class IClientChangeOperation;
	class IReplicationDiscoverer;
	
	struct FChangeClientReplicationRequest;
	
	/** Interface for interacting with Multi-User replication, which uses the Concert replication system. */
	class MULTIUSERCLIENT_API IMultiUserReplication
	{
	public:

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

		/**
		 * Enqueues a request for changing a client's stream and authority.
		 * The request is enqueued with the other requests that Multi-User might have ongoing already (like those triggered by the UI).
		 *
		 * A stream is the mapping of objects to properties.
		 * The authority state specifies which of the registered objects should actually be sending data.
		 * The stream change is requested first and is followed by the authority change.
		 *
		 * @param ClientId The client for which to change authority
		 * @param SubmissionParams Once the request is ready to be sent to the server, this attribute is used to generate the change request
		 */
		virtual TSharedRef<IClientChangeOperation> EnqueueChanges(const FGuid& ClientId, TAttribute<FChangeClientReplicationRequest> SubmissionParams) = 0;

		virtual ~IMultiUserReplication() = default;
	};
}

