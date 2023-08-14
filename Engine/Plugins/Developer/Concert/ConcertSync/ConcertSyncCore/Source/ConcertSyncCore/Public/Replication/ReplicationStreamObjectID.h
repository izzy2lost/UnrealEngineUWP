// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/Guid.h"
#include "UObject/SoftObjectPath.h"

namespace UE::ConcertSyncCore
{
	struct FStreamedObjectID
	{
		/** The replication stream that produces data for this object. */
		FGuid StreamId;
		/** The object to process */
		FSoftObjectPath Object;
		
		friend bool operator==(const FStreamedObjectID& Left, const FStreamedObjectID& Right)
		{
			return Left.StreamId == Right.StreamId && Left.Object == Right.Object;
		}

		friend bool operator!=(const FStreamedObjectID& Left, const FStreamedObjectID& Right)
		{
			return !(Left == Right);
		}
	};
	
	struct FReplicatedObjectInfo : FStreamedObjectID
	{
		/** The ID of the endpoint that sent this object. This can be the client or server endpoint depending on who receives it. */
		FGuid SenderEndpointId;
		
		friend bool operator==(const FReplicatedObjectInfo& Left, const FReplicatedObjectInfo& Right)
		{
			return Left.SenderEndpointId == Right.SenderEndpointId
				&& static_cast<const FStreamedObjectID&>(Left) == static_cast<const FStreamedObjectID&>(Right);
		}

		friend bool operator!=(const FReplicatedObjectInfo& Left, const FReplicatedObjectInfo& Right)
		{
			return !(Left == Right);
		}
	};

	CONCERTSYNCCORE_API uint32 GetTypeHash(const FStreamedObjectID& StreamObject);
	CONCERTSYNCCORE_API uint32 GetTypeHash(const FReplicatedObjectInfo& StreamObject);
}