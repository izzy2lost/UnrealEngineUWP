// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ReplicationClient.h"

namespace UE::MultiUserClient
{
	class FRegularQueryService;
	
	/** Holds extra information about a remote replication client. */
	class FRemoteReplicationClient : public FReplicationClient
	{
	public:

		FRemoteReplicationClient(
			const FGuid& InConcertClientId,
			UMultiUserReplicationClientPreset& InSessionContent,
			FRegularQueryService& QueryService
			);
	};
}

