// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ReplicationClient.h"

namespace UE::MultiUserClient
{
	class FGlobalAuthorityCache;

	/** Holds extra information about a local replication client. */
	class FLocalReplicationClient : public FReplicationClient
	{
	public:

		FLocalReplicationClient(
			FGlobalAuthorityCache& InAuthorityCache,
			UMultiUserReplicationClientPreset& InSessionContent,
			TUniquePtr<IClientStreamSynchronizer> InStreamSynchronizer,
			TSharedRef<IConcertSyncClient> InClient
			);
	};
}

