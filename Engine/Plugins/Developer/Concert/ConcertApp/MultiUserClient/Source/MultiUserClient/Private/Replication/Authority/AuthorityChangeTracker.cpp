// Copyright Epic Games, Inc. All Rights Reserved.

#include "AuthorityChangeTracker.h"

#include "IClientAuthoritySynchronizer.h"

namespace UE::MultiUserClient
{
	FAuthorityChangeTracker::FAuthorityChangeTracker(
		IClientAuthoritySynchronizer& InAuthoritySynchronizer
		)
		: AuthoritySynchronizer(InAuthoritySynchronizer)
	{
		AuthoritySynchronizer.OnServerStateChanged().AddRaw(this, &FAuthorityChangeTracker::RefreshChanges);
	}

	FAuthorityChangeTracker::~FAuthorityChangeTracker()
	{
		// Strictly not needed because AuthoritySynchronizer will die at the same time as us but let's be nice
		AuthoritySynchronizer.OnServerStateChanged().RemoveAll(this);
	}

	void FAuthorityChangeTracker::SetAuthorityIfAllowed(const FSoftObjectPath& ObjectPath, bool bNewAuthorityState)
	{
		if (CanSetAuthorityFor(ObjectPath))
		{
			NewAuthorityStates.Add(ObjectPath, bNewAuthorityState);
		}
	}

	void FAuthorityChangeTracker::RefreshChanges()
	{
		for (auto It = NewAuthorityStates.CreateIterator(); It; ++It)
		{
			const TPair<FSoftObjectPath, bool>& NewAuthorityState = *It;
			const FSoftObjectPath& ObjectPath = NewAuthorityState.Key;

			const bool bHasAuthority = AuthoritySynchronizer.HasAuthorityOver(ObjectPath); 
			if (bHasAuthority == NewAuthorityState.Value
				|| !AuthoritySynchronizer.CanChangeAuthority(ObjectPath))
			{
				It.RemoveCurrent();
			}
		}
	}

	bool FAuthorityChangeTracker::GetAuthorityStateAfterApplied(const FSoftObjectPath& ObjectPath) const
	{
		const bool* ChangeValue = NewAuthorityStates.Find(ObjectPath);
		return ChangeValue ? *ChangeValue : AuthoritySynchronizer.HasAuthorityOver(ObjectPath);
	}

	bool FAuthorityChangeTracker::CanSetAuthorityFor(const FSoftObjectPath& ObjectPath) const
	{
		return AuthoritySynchronizer.CanChangeAuthority(ObjectPath);
	}

	EAuthorityMutability FAuthorityChangeTracker::GetChangeAuthorityMutability(const FSoftObjectPath& ObjectPath) const
	{
		return AuthoritySynchronizer.GetChangeAuthorityMutability(ObjectPath);
	}

	ConcertSyncClient::Replication::FAuthorityChangeRequest FAuthorityChangeTracker::BuildChangeRequest(const FGuid& StreamId) const
	{
		ConcertSyncClient::Replication::FAuthorityChangeRequest ChangeRequest;

		for (const TPair<FSoftObjectPath, bool>& NewAuthorityState : NewAuthorityStates)
		{
			if (NewAuthorityState.Value)
			{
				ChangeRequest.TakeAuthority.Add(NewAuthorityState.Key).StreamIds = { StreamId };
			}
			else
			{
				ChangeRequest.ReleaseAuthority.Add(NewAuthorityState.Key).StreamIds = { StreamId };
			}
		}
		
		return ChangeRequest;
	}
}
