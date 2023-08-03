// Copyright Epic Games, Inc. All Rights Reserved.

#include "Replication/ReplicationStreamObjectID.h"

namespace UE::ConcertSyncCore
{
	uint32 GetTypeHash(const FReplicationStreamObjectID& StreamObject)
	{
		return HashCombineFast(GetTypeHash(StreamObject.Object), GetTypeHash(StreamObject.StreamId));
	}
}