// Copyright Epic Games, Inc. All Rights Reserved.

#include "StreamSynchronizer_LocalClient.h"

#include "IConcertSyncClient.h"
#include "Replication/IConcertClientReplicationManager.h"
#include "Replication/Messages/ChangeStream.h"

#define LOCTEXT_NAMESPACE "FLocalClientStreamDiffer"

namespace UE::MultiUserClient
{
	FStreamSynchronizer_LocalClient::FStreamSynchronizer_LocalClient(
		TSharedRef<IConcertSyncClient> InLocalClient,
		const FGuid& InLocalClientStreamId
		)
		: LocalClient(MoveTemp(InLocalClient))
		, LocalClientStreamId(InLocalClientStreamId)
	{
		IConcertClientReplicationManager* ReplicationManager = LocalClient->GetReplicationManager();
		if (ensure(ReplicationManager))
		{
			ReplicationManager->OnPostStreamsChanged().AddRaw(this, &FStreamSynchronizer_LocalClient::OnPostStreamsChanged);
		}
	}

	FStreamSynchronizer_LocalClient::~FStreamSynchronizer_LocalClient()
	{
		if (IConcertClientReplicationManager* ReplicationManager = LocalClient->GetReplicationManager())
		{
			ReplicationManager->OnPostStreamsChanged().RemoveAll(this);
		}
	}

	const FObjectReplicationMap& FStreamSynchronizer_LocalClient::GetServerState() const
	{
		IConcertClientReplicationManager* ReplicationManager = LocalClient->GetReplicationManager();
		if (!ensure(ReplicationManager))
		{
			return EmptyState;
		}
		
		const FObjectReplicationMap* Result = nullptr;
		ReplicationManager->ForEachRegisteredStream([this, &Result](const FReplicationStreamDescription& Stream)
		{
			if (Stream.BaseDescription.Identifier == LocalClientStreamId)
			{
				Result = &Stream.BaseDescription.ReplicationMap;
				return EBreakBehavior::Break;
			}
			return EBreakBehavior::Continue;
		});
		return Result
			? *Result
			: EmptyState;
	}

	void FStreamSynchronizer_LocalClient::OnPostStreamsChanged()
	{
		// Note that this fires even for non MU-related changes, e.g. due to some other system's Concert replication API usage.
		OnServerStateChangedDelegate.Broadcast();
	}
}

#undef LOCTEXT_NAMESPACE