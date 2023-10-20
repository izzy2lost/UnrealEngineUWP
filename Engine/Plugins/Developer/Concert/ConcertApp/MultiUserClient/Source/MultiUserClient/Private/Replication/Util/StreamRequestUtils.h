// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Replication/IConcertClientReplicationManager.h"

namespace UE::ConcertSyncClient::Replication
{
	struct FChangeStreamRequest;
}

namespace UE::MultiUserClient
{
	struct FStreamChangelist;
}

namespace UE::MultiUserClient::StreamRequestUtils
{
	/**
	 * Builds a request for creating a new stream.
	 * @param StreamId The stream that should be modified
	 * @param FromChangelist The changes to be made
	 */
	ConcertSyncClient::Replication::FChangeStreamRequest BuildChangeRequest_CreateNewStream(const FGuid& StreamId, const FStreamChangelist& FromChangelist);
	
	/**
	 * Builds a request for updating a preexisting stream.
	 * @param FromChangelist The changes to be made
	 */
	ConcertSyncClient::Replication::FChangeStreamRequest BuildChangeRequest_UpdateExistingStream(FStreamChangelist FromChangelist);
}
