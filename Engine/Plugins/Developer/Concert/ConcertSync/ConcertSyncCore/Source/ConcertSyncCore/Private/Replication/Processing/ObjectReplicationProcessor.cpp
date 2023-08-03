// Copyright Epic Games, Inc. All Rights Reserved.

#include "Replication/Processing/ObjectReplicationProcessor.h"

#include "ConcertMessageData.h"
#include "Replication/Processing/IReplicationDataSource.h"

namespace UE::ConcertSyncCore
{
	FObjectReplicationProcessor::FObjectReplicationProcessor(TSharedRef<IReplicationDataSource> DataSource)
		: DataSource(MoveTemp(DataSource))
	{}

	void FObjectReplicationProcessor::ProcessObjects(float TimeBudget)
	{
		// TODO: Respect time budget and prioritize objects
		DataSource->ForEachPendingObject([this](const FReplicationStreamObjectID& ObjectInfo)
		{
			ProcessObject({ ObjectInfo });
		});
	}
}
