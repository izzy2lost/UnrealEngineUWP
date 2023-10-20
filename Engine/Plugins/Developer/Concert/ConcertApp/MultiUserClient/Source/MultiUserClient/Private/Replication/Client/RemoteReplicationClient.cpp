// Copyright Epic Games, Inc. All Rights Reserved.

#include "RemoteReplicationClient.h"

#include "Assets/MultiUserReplicationClientPreset.h"
#include "Replication/Authority/AuthoritySynchronizer_RemoteClient.h"
#include "Replication/Stream/StreamSynchronizer_RemoteClient.h"
#include "Replication/Submission/SubmissionWorkflow_RemoteClient.h"

namespace UE::MultiUserClient
{
	FRemoteReplicationClient::FRemoteReplicationClient(
		UMultiUserReplicationClientPreset& InSessionContent,
		const FGuid& InConcertClientId,
		FRegularQueryService& QueryService
		)
		: FReplicationClient(
			InSessionContent,
			MakeUnique<FStreamSynchronizer_RemoteClient>(InConcertClientId, QueryService),
			MakeUnique<FAuthoritySynchronizer_RemoteClient>(
				InConcertClientId,
				QueryService,
				FDoesObjectHaveProperties::CreateLambda([this](const FSoftObjectPath& ObjectPath)
				{
					return GetStreamDiffer().DoesObjectHavePropertiesAfterSubmit(ObjectPath);
				})),
			[]() { return MakeUnique<FSubmissionWorkflow_RemoteClient>(); }
			)
		, RemoteEndpointId(InConcertClientId)
	{
		// When the remote client's state has changed, refresh the UI.
		GetStreamSynchronizer().OnServerStateChanged().AddLambda([this]()
		{
			GetClientContent()->Stream->ReplicationMap = GetStreamSynchronizer().GetServerState();
			OnModelExternallyChanged().Broadcast();
		});
	}
}
