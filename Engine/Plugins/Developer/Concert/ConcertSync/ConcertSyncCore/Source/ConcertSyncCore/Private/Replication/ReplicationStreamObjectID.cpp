// Copyright Epic Games, Inc. All Rights Reserved.

#include "Replication/ReplicationStreamObjectID.h"

namespace UE::ConcertSyncCore
{
	uint32 GetTypeHash(const FStreamedObjectID& StreamObject)
	{
		return HashCombineFast(GetTypeHash(StreamObject.Object), GetTypeHash(StreamObject.StreamId));
	}
	
	uint32 GetTypeHash(const FReplicatedObjectInfo& StreamObject)
	{
		return HashCombineFast(GetTypeHash(StreamObject.SenderEndpointId), GetTypeHash(static_cast<FStreamedObjectID>(StreamObject)));
	}
}