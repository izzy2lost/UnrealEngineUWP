// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#if WITH_EDITOR

#include "CoreMinimal.h"
#include "WorldPartition/WorldPartitionActorDescInstanceInterface.h"

class AActor;
class IStreamingGenerationErrorHandler;
class UWorldPartition;
struct FWorldPartitionActorFilter;
enum class EWorldPartitionActorFilterType : uint8;

#endif // WITH_EDITOR

/**
 * Interface for a view on top of an actor desc, used to cache information that can be (potentially) different than the actor desc
 * itself due to streaming generation logic, etc.
 */
class IWorldPartitionActorDescInstanceView : public IWorldPartitionActorDescInstance
{
public:
	virtual void CheckForErrors(IStreamingGenerationErrorHandler* ErrorHandler) const = 0;
};

