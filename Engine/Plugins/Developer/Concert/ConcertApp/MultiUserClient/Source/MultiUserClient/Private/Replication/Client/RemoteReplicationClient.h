// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ReplicationClient.h"

class IConcertClient;

namespace UE::MultiUserClient
{
	class FRegularQueryService;
	
	/** Holds extra information about a remote replication client. */
	class FRemoteReplicationClient : public FReplicationClient
	{
	public:

		FRemoteReplicationClient(
			const FGuid& InConcertClientId,
			TSharedRef<IConcertClient> InClient,
			FGlobalAuthorityCache& InAuthorityCache,
			UMultiUserReplicationClientPreset& InSessionContent,
			FRegularQueryService& QueryService
			);
	};
}

