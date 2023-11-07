// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Map.h"
#include "Containers/ContainersFwd.h"
#include "Replication/IConcertClientReplicationManager.h"

namespace UE::MultiUserClient
{
	class FGlobalAuthorityCache;
}

struct FSoftObjectPath;

namespace UE::MultiUserClient
{
	class IClientAuthoritySynchronizer;
	enum class EAuthorityMutability;
	
	/** Keeps track of authority changes the local editor instance makes to a client. */
	class FAuthorityChangeTracker
	{
	public:
		
		FAuthorityChangeTracker(const FGuid& InClientId, const IClientAuthoritySynchronizer& InAuthoritySynchronizer, FGlobalAuthorityCache& InAuthorityCache);
		~FAuthorityChangeTracker();
		
		/**
		 * Marks that the authority should be changed to bNewAuthorityState.
		 */
		void SetAuthorityIfAllowed(TConstArrayView<FSoftObjectPath> ObjectPaths, bool bNewAuthorityState);

		/** Diffs NewAuthorityStates to the current authority states and removes entries. */
		void RefreshChanges();
		
		void ClearChanges() { NewAuthorityStates.Reset(); }
		bool HasChanges() const { return !NewAuthorityStates.IsEmpty(); }
		
		/** Gets the authority state the object will have if the changes are applied. */
		bool GetAuthorityStateAfterApplied(const FSoftObjectPath& ObjectPath) const;
		
		/** @return Whether it is valid to set authority for the given ObjectPath: that is if the object has properties registered. */
		bool CanSetAuthorityFor(const FSoftObjectPath& ObjectPath) const;
		/** @return Detailed information why the object's authority can or cannot be changed.  */
		EAuthorityMutability GetChangeAuthorityMutability(const FSoftObjectPath& ObjectPath) const;

		/** Builds a change request from the local changes. */
		ConcertSyncClient::Replication::FAuthorityChangeRequest BuildChangeRequest(const FGuid& StreamId) const;

		/** Called when NewAuthorityStates is updated. */
		DECLARE_MULTICAST_DELEGATE(FOnAuthorityChangeMade);
		FOnAuthorityChangeMade& OnAuthorityChangeMade() { return FOnAuthorityChangeMadeDelegate; }
		
	private:

		/** Id of the client this change tracker is tracking. */
		const FGuid ClientId;

		/** Knows the current authority state of the client and determines whether we support changing this client's authority at all. */
		const IClientAuthoritySynchronizer& AuthoritySynchronizer;
		/** Used to determine whether other clients have authority over objects. */
		FGlobalAuthorityCache& AuthorityCache;

		/** Object to the authority state it should have. */
		TMap<FSoftObjectPath, bool> NewAuthorityStates;

		/** Called when NewAuthorityStates is updated. */
		FOnAuthorityChangeMade FOnAuthorityChangeMadeDelegate;
		
		void OnClientChanged(const FGuid& Guid) { RefreshChanges(); }
	};
}


