// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Replication/Data/ConcertPropertySelection.h"
#include "ObjectReplicationMap.generated.h"

USTRUCT()
struct FReplicatedObjectInfo
{
	GENERATED_BODY()
	
	UPROPERTY()
	FSoftClassPath ClassPath;
	
	UPROPERTY()
	FConcertPropertySelection PropertySelection;

	/** @return Whether this data is valid for sending to the server. */
	bool IsValidForSendingToServer() const { return ClassPath.IsValid() && !PropertySelection.ReplicatedProperties.IsEmpty(); }
	
	friend bool operator==(const FReplicatedObjectInfo& Left, const FReplicatedObjectInfo& Right)
	{
		return Left.ClassPath == Right.ClassPath
			&& Left.PropertySelection == Right.PropertySelection;
	}
	friend bool operator!=(const FReplicatedObjectInfo& Left, const FReplicatedObjectInfo& Right)
	{
		return !(Left == Right);
	}
};

/** Maps objects to their replicated properties. */
USTRUCT()
struct FObjectReplicationMap
{
	GENERATED_BODY()

	/**
	 * List of replicated objects.
	 *
	 * Can be actors or its subobjects.
	 * Technically, this can also be non-UWorld objects.
	 */
	UPROPERTY()
	TMap<FSoftObjectPath, FReplicatedObjectInfo> ReplicatedObjects;

	friend bool operator==(const FObjectReplicationMap& Left, const FObjectReplicationMap& Right)
	{
		return Left.ReplicatedObjects.OrderIndependentCompareEqual(Right.ReplicatedObjects);
	}
	friend bool operator!=(const FObjectReplicationMap& Left, const FObjectReplicationMap& Right)
	{
		return !(Left == Right);
	}
};