// Copyright Epic Games, Inc. All Rights Reserved.

#include "RemoteReplicationClient.h"

#include "Assets/MultiUserReplicationClientPreset.h"
#include "Replication/Authority/AuthoritySynchronizer_RemoteClient.h"
#include "Replication/Stream/StreamSynchronizer_RemoteClient.h"
#include "Replication/Submission/SubmissionWorkflow_RemoteClient.h"

namespace UE::MultiUserClient
{
	FRemoteReplicationClient::FRemoteReplicationClient(
		const FGuid& InConcertClientId,
		FGlobalAuthorityCache& InAuthorityCache,
		UMultiUserReplicationClientPreset& InSessionContent,
		FRegularQueryService& QueryService
		)
		: FReplicationClient(
			InConcertClientId,
			InAuthorityCache,
			InSessionContent,
			MakeUnique<FStreamSynchronizer_RemoteClient>(InConcertClientId, QueryService),
			MakeUnique<FAuthoritySynchronizer_RemoteClient>(
				InConcertClientId,
				QueryService,
				FDoesObjectHaveProperties::CreateLambda([this](const FSoftObjectPath& ObjectPath)
				{
					return GetStreamDiffer().DoesObjectHavePropertiesAfterSubmit(ObjectPath);
				})),
			MakeUnique<FSubmissionWorkflow_RemoteClient>())
	{
		// When the remote client's state has changed, refresh the UI.
		GetStreamSynchronizer().OnServerStateChanged().AddLambda([this]()
		{
			GetClientContent()->Stream->ReplicationMap = GetStreamSynchronizer().GetServerState();
		});
	}
}
