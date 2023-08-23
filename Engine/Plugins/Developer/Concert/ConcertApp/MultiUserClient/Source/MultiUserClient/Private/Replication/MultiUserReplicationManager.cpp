// Copyright Epic Games, Inc. All Rights Reserved.

#include "MultiUserReplicationManager.h"

#include "IConcertSyncClient.h"
#include "MultiUserReplicationClientProfileAsset.h"
#include "ReplicationUtils.h"

namespace UE::MultiUserClient
{
	FMultiUserReplicationManager::FMultiUserReplicationManager(TSharedRef<IConcertSyncClient> InClient)
		: Client(MoveTemp(InClient))
	{}

	void FMultiUserReplicationManager::JoinReplicationSession(const UMultiUserReplicationClientProfileAsset& Asset)
	{
		Replication::JoinSessionForMultiUser(Client, Asset.ToJoinArgs());
	}

	void FMultiUserReplicationManager::LeaveSession()
	{
		if (IConcertClientReplicationManager* ReplicationManager = Client->GetReplicationManager())
		{
			ReplicationManager->LeaveReplicationSession();
		}
	}

	bool FMultiUserReplicationManager::CanJoin() const
	{
		IConcertClientReplicationManager* ReplicationManager = Client->GetReplicationManager();
		return ReplicationManager && ReplicationManager->CanJoin();
	}

	bool FMultiUserReplicationManager::IsConnectedToReplicationSession() const
	{
		IConcertClientReplicationManager* ReplicationManager = Client->GetReplicationManager();
		return ReplicationManager && ReplicationManager->IsConnectedToReplicationSession();
	}
}
