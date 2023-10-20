// Copyright Epic Games, Inc. All Rights Reserved.

#include "AuthoritySynchronizer_LocalClient.h"

#include "IConcertSyncClient.h"
#include "Replication/IConcertClientReplicationManager.h"

namespace UE::MultiUserClient
{
	FAuthoritySynchronizer_LocalClient::FAuthoritySynchronizer_LocalClient(
		TSharedRef<IConcertSyncClient> InClient,
		FDoesObjectHaveProperties InDoesObjectHaveProperties
		)
		: FAuthoritySynchronizer_Base(MoveTemp(InDoesObjectHaveProperties))
		, Client(MoveTemp(InClient))
	{
		IConcertClientReplicationManager* ReplicationManager = Client->GetReplicationManager();
		if (ensure(ReplicationManager))
		{
			ReplicationManager->OnPostAuthorityChanged().AddRaw(this, &FAuthoritySynchronizer_LocalClient::OnAuthorityChanged);
		}
	}

	FAuthoritySynchronizer_LocalClient::~FAuthoritySynchronizer_LocalClient()
	{
		if (IConcertClientReplicationManager* ReplicationManager = Client->GetReplicationManager())
		{
			ReplicationManager->OnPostAuthorityChanged().RemoveAll(this);
		}
	}

	bool FAuthoritySynchronizer_LocalClient::HasAuthorityOver(const FSoftObjectPath& ObjectPath) const
	{
		const IConcertClientReplicationManager* ReplicationManager = Client->GetReplicationManager();
		return ensure(ReplicationManager) && !ReplicationManager->GetClientOwnedStreamsForObject(ObjectPath).IsEmpty();
	}

	void FAuthoritySynchronizer_LocalClient::OnAuthorityChanged() const
	{
		OnServerStateChangedDelegate.Broadcast();
	}
}
