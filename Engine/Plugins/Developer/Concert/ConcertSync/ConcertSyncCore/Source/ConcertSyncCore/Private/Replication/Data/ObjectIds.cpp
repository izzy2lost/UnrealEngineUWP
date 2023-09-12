// Copyright Epic Games, Inc. All Rights Reserved.

#include "Replication/Data/ObjectIds.h"

uint32 GetTypeHash(const FObjectInStreamID& StreamObject)
{
	return HashCombineFast(GetTypeHash(StreamObject.Object), GetTypeHash(StreamObject.StreamId));
}

uint32 GetTypeHash(const FReplicatedObjectId& StreamObject)
{
	return HashCombineFast(GetTypeHash(StreamObject.SenderEndpointId), GetTypeHash(static_cast<FObjectInStreamID>(StreamObject)));
}