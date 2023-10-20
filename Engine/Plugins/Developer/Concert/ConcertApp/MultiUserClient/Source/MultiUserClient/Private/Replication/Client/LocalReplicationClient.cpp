// Copyright Epic Games, Inc. All Rights Reserved.

#include "LocalReplicationClient.h"

#include "Replication/Authority/AuthoritySynchronizer_LocalClient.h"
#include "Replication/Submission/SubmissionWorkflow_LocalClient.h"

namespace UE::MultiUserClient
{
	FLocalReplicationClient::FLocalReplicationClient(
		UMultiUserReplicationClientPreset& InSessionContent,
		TUniquePtr<IClientStreamSynchronizer> InStreamSynchronizer,
		TSharedRef<IConcertSyncClient> InClient
		)
		: FReplicationClient(
			InSessionContent,
			MoveTemp(InStreamSynchronizer),
			MakeUnique<FAuthoritySynchronizer_LocalClient>(
				InClient,
				FDoesObjectHaveProperties::CreateLambda([this](const FSoftObjectPath& ObjectPath)
				{
					return GetStreamDiffer().DoesObjectHavePropertiesAfterSubmit(ObjectPath);
				})),
			[this, InClient]()
			{
				return MakeUnique<FSubmissionWorkflow_LocalClient>(InClient, GetStreamDiffer(), GetAuthorityDiffer(), GetStreamSynchronizer());
			})
	{}
}
