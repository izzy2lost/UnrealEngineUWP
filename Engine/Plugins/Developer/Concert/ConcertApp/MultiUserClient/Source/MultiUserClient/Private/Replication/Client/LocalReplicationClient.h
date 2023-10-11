// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ReplicationClient.h"
#include "Replication/Authority/AuthorityPolicy.h"

namespace UE::MultiUserClient
{
	/** Holds extra information about a local replication client. */
	class FLocalReplicationClient : public FReplicationClient
	{
	public:

		FLocalReplicationClient(
			UMultiUserReplicationClientPreset& InSessionContent,
			TUniquePtr<IClientStreamSynchronizer> InStreamSynchronizer,
			TSharedRef<IConcertSyncClient> InClient
			);

		FAuthorityPolicy& GetAuthorityPolicy() { return AuthorityPolicy; }

	private:

		/** Automatically requests authority for submitted, locally-owned objects. */
		FAuthorityPolicy AuthorityPolicy;
	};
}

