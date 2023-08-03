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
};