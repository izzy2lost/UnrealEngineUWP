// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Map.h"
#include "Replication/IConcertClientReplicationManager.h"

struct FSoftObjectPath;

namespace UE::MultiUserClient
{
	class IClientAuthoritySynchronizer;
	enum class EAuthorityMutability;
	
	/** Keeps track of authority changes the local editor instance makes to a client. */
	class FAuthorityChangeTracker
	{
	public:

		FAuthorityChangeTracker(IClientAuthoritySynchronizer& InAuthoritySynchronizer);
		~FAuthorityChangeTracker();
		
		/** Marks that the authority should be changed to bNewAuthorityState. */
		void SetAuthorityIfAllowed(const FSoftObjectPath& ObjectPath, bool bNewAuthorityState);

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
		
	private:

		/** Knows the current authority state of the client. */
		IClientAuthoritySynchronizer& AuthoritySynchronizer;

		/** Object to the authority state it should have. */
		TMap<FSoftObjectPath, bool> NewAuthorityStates;
	};
}


