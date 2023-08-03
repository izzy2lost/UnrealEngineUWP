// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/Guid.h"
#include "UObject/SoftObjectPath.h"

namespace UE::ConcertSyncCore
{
	struct FReplicationStreamObjectID
	{
		/** The replication stream that produces data for this object. */
		FGuid StreamId;
		/** The object to process */
		FSoftObjectPath Object;

		friend bool operator==(const FReplicationStreamObjectID& Left, const FReplicationStreamObjectID& Right)
		{
			return Left.StreamId == Right.StreamId && Left.Object == Right.Object;
		}

		friend bool operator!=(const FReplicationStreamObjectID& Left, const FReplicationStreamObjectID& Right)
		{
			return !(Left == Right);
		}
	};

	CONCERTSYNCCORE_API uint32 GetTypeHash(const FReplicationStreamObjectID& StreamObject);
}