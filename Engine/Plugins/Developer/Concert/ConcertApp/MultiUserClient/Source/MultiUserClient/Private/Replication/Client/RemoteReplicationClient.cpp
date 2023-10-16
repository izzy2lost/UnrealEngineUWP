// Copyright Epic Games, Inc. All Rights Reserved.

#include "RemoteReplicationClient.h"

#include "Assets/MultiUserReplicationClientPreset.h"
#include "Replication/Stream/RemoteClientStreamSynchronizer.h"

namespace UE::MultiUserClient
{
	FRemoteReplicationClient::FRemoteReplicationClient(
		UMultiUserReplicationClientPreset& InSessionContent,
		const FGuid& InConcertClientId,
		FRegularQueryService& QueryService
		)
		: FReplicationClient(InSessionContent, MakeUnique<FRemoteClientStreamSynchronizer>(InConcertClientId, QueryService))
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
