// Copyright Epic Games, Inc. All Rights Reserved.

#include "LocalReplicationClient.h"

namespace UE::MultiUserClient
{
	FLocalReplicationClient::FLocalReplicationClient(
		UMultiUserReplicationClientPreset& InSessionContent,
		TUniquePtr<IClientStreamSynchronizer> InStreamSynchronizer,
		TSharedRef<IConcertSyncClient> InClient
		)
		: FReplicationClient(InSessionContent, MoveTemp(InStreamSynchronizer))
		, AuthorityPolicy(MoveTemp(InClient), GetStreamSynchronizer())
	{
	}
}
