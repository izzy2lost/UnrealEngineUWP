// Copyright Epic Games, Inc. All Rights Reserved.

#include "LocalReplicationClient.h"

#include "IConcertSyncClient.h"
#include "Replication/Authority/AuthoritySynchronizer_LocalClient.h"
#include "Replication/Submission/SubmissionWorkflow_LocalClient.h"

namespace UE::MultiUserClient
{
	FLocalReplicationClient::FLocalReplicationClient(
		FGlobalAuthorityCache& InAuthorityCache,
		UMultiUserReplicationClientPreset& InSessionContent,
		TUniquePtr<IClientStreamSynchronizer> InStreamSynchronizer,
		TSharedRef<IConcertSyncClient> InClient
		)
		: FReplicationClient(
			InClient->GetConcertClient()->GetCurrentSession()->GetSessionClientEndpointId(),
			InAuthorityCache,
			InSessionContent,
			MoveTemp(InStreamSynchronizer),
		MakeUnique<FAuthoritySynchronizer_LocalClient>(
			InClient,
			FDoesObjectHaveProperties::CreateLambda([this](const FSoftObjectPath& ObjectPath)
			{
				return GetStreamDiffer().DoesObjectHavePropertiesAfterSubmit(ObjectPath);
			})),
			MakeUnique<FSubmissionWorkflow_LocalClient>(MoveTemp(InClient)))
	{}
}
