// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Replication/Data/ConcertPropertySelection.h"
#include "UObject/Object.h"
#include "ObjectReplicationMap.generated.h"

USTRUCT()
struct FReplicatedObjectInfo
{
	GENERATED_BODY()
	
	UPROPERTY()
	FSoftClassPath ClassPath;
	
	UPROPERTY()
	FConcertPropertySelection PropertySelection;

	// Use static factories instead of user-provided constructors to continue allowing brace-initialization
	static FReplicatedObjectInfo Make(const UObject& Object)
	{
		return { Object.GetClass() };
	}

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

	/** @return Whether ObjectPath has any properties assigned to it. */
	bool HasProperties(const FSoftObjectPath& ObjectPath) const
	{
		const FReplicatedObjectInfo* ObjectInfo = ReplicatedObjects.Find(ObjectPath);
		return ObjectInfo && !ObjectInfo->PropertySelection.ReplicatedProperties.IsEmpty();
	}

	/** @return Whether there are any properties assigned. */
	bool IsEmpty() const
	{
		return ReplicatedObjects.IsEmpty();
	}

	friend bool operator==(const FObjectReplicationMap& Left, const FObjectReplicationMap& Right)
	{
		return Left.ReplicatedObjects.OrderIndependentCompareEqual(Right.ReplicatedObjects);
	}
	friend bool operator!=(const FObjectReplicationMap& Left, const FObjectReplicationMap& Right)
	{
		return !(Left == Right);
	}
};