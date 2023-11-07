// Copyright Epic Games, Inc. All Rights Reserved.

#include "LocalReplicationClient.h"

#include "IConcertSyncClient.h"
#include "Replication/Authority/AuthoritySynchronizer_LocalClient.h"
#include "Replication/Submission/SubmissionWorkflow_LocalClient.h"

namespace UE::MultiUserClient
{
	FLocalReplicationClient::FLocalReplicationClient(
		UMultiUserReplicationClientPreset& InSessionContent,
		TUniquePtr<IClientStreamSynchronizer> InStreamSynchronizer,
		TSharedRef<IConcertSyncClient> InClient,
		const FGlobalAuthorityCache& InAuthorityCache
		)
		: FReplicationClient(
			InClient->GetConcertClient()->GetCurrentSession()->GetSessionClientEndpointId(),
			InSessionContent,
			MoveTemp(InStreamSynchronizer),
		MakeUnique<FAuthoritySynchronizer_LocalClient>(
			InClient,
			FDoesObjectHaveProperties::CreateLambda([this](const FSoftObjectPath& ObjectPath)
			{
				return GetStreamDiffer().DoesObjectHavePropertiesAfterSubmit(ObjectPath);
			})),
			[this, InClient, &InAuthorityCache](FStreamChangeTracker& InStreamChangeTracker, FAuthorityChangeTracker& InAuthorityChangeTracker, IClientStreamSynchronizer& InStreamSynchronizer) mutable
			{
				return MakeUnique<FSubmissionWorkflow_LocalClient>(MoveTemp(InClient), InStreamChangeTracker, InAuthorityChangeTracker, InStreamSynchronizer, InAuthorityCache);
			})
	{}
}
