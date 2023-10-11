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
			UMultiUserReplicationClientPreset& InSessionContent,
			const FGuid& InConcertClientId,
			FRegularQueryService& QueryService
			);
		
		const FGuid& GetRemoteEndpointId() const { return RemoteEndpointId; }
		
	private:

		/**
		 * Endpoint of the Concert client.
		 *
		 * Can be used to look up client info.
		 * @see IConcertSession::FindSessionClient
		 */
		const FGuid RemoteEndpointId;
	};
}

