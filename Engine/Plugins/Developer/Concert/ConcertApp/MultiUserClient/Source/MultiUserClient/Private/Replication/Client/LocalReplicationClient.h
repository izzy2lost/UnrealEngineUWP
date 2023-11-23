// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ReplicationClient.h"
#include "Replication/Submission/Remote/RemoteSubmissionListener.h"

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

	private:
		
		/** Listens for and handles for submission request made by remote clients' SubmissionWorkflow.*/
		FRemoteSubmissionListener RemoteSubmissionListener;
	};
}

