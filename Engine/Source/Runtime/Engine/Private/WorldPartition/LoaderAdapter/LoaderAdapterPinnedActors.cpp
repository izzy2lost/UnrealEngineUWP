// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartition/LoaderAdapter/LoaderAdapterPinnedActors.h"

#if WITH_EDITOR

#include "Engine/World.h"
#include "Engine/Level.h"
#include "WorldPartition/WorldPartition.h"

#define LOCTEXT_NAMESPACE "FLoaderAdapterPinnedActors"

namespace LoaderAdapterPinnedActorsUtils
{
	static bool SupportsPinning(FWorldPartitionActorDesc* InActorDesc, bool bCheckIsMainWorldPartition)
	{
		if (!InActorDesc)
		{
			return false;
		}

		// Only Spatially loaded actors can be pinned with the exception of non spatially loaded runtime only actors (ex: HLODs)
		if (!InActorDesc->GetIsSpatiallyLoaded() && !InActorDesc->GetActorIsRuntimeOnly())
		{
			return false;
		}

		// This allows skipping actors that can never be loaded in current context: ex: bActorShouldSkipLevelInstance
		if (!InActorDesc->IsEditorRelevant() && !InActorDesc->GetActorIsRuntimeOnly())
		{
			return false;
		}

		if (UActorDescContainer* Container = InActorDesc->GetContainer())
		{
			const UWorldPartition* ContainerWorldPartition = Container->GetWorldPartition();
			return ContainerWorldPartition && (ContainerWorldPartition->IsMainWorldPartition() || !bCheckIsMainWorldPartition);
		}

		return false;
	}
}

bool FLoaderAdapterPinnedActors::PassActorDescFilter(const FWorldPartitionHandle& ActorHandle) const
{
	// We want to be able to pin any type of actors (HLODs, etc).
	// Allow recursive pinning by setting bCheckIsMainWorldPartition = false
	const bool bCheckIsMainWorldPartition = false;
	return ActorHandle.IsValid() && !ActorsToRemove.Contains(ActorHandle) && LoaderAdapterPinnedActorsUtils::SupportsPinning(ActorHandle.Get(), bCheckIsMainWorldPartition);
}



bool FLoaderAdapterPinnedActors::SupportsPinning(FWorldPartitionActorDesc* InActorDesc)
{
	// Public api doesn't allow pinning of non main world partition actors
	const bool bCheckIsMainWorldPartition = true;
	return LoaderAdapterPinnedActorsUtils::SupportsPinning(InActorDesc, bCheckIsMainWorldPartition);
}

bool FLoaderAdapterPinnedActors::SupportsPinning(AActor* InActor)
{
	if (InActor)
	{
		// Pinning of Actors is only supported on the main world partition
		const ULevel* Level = InActor->GetLevel();
		const UWorld* World = Level->GetWorld();

		// Only Spatially loaded actors can be pinned with the exception of non spatially loaded, runtime only actors (ex: HLODs)
		return World && !World->IsGameWorld() && !!World->GetWorldPartition() && Level->IsPersistentLevel() && InActor->IsPackageExternal() && (InActor->GetIsSpatiallyLoaded() || InActor->IsRuntimeOnly());
	}

	return false;
}

#undef LOCTEXT_NAMESPACE

#endif
