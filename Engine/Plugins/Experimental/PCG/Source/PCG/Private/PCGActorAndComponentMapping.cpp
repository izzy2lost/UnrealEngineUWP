// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGActorAndComponentMapping.h"

#include "PCGComponent.h"
#include "PCGGraph.h"
#include "PCGModule.h"
#include "PCGSubsystem.h"
#include "PCGWorldActor.h"
#include "Grid/PCGPartitionActor.h"
#include "Helpers/PCGActorHelpers.h"
#include "Helpers/PCGHelpers.h"

#include "Landscape.h"
#include "LandscapeProxy.h"
#include "StaticMeshCompiler.h"
#include "TextureCompiler.h"
#include "Algo/AnyOf.h"
#include "Engine/DataTable.h"
#include "Engine/Engine.h"
#include "Engine/Level.h"
#include "Engine/StaticMesh.h"
#include "Kismet/GameplayStatics.h"
#include "LevelInstance/LevelInstanceInterface.h"
#include "LevelInstance/LevelInstanceSubsystem.h"
#include "LevelInstance/LevelInstanceEditorInstanceActor.h"
#include "Materials/MaterialInterface.h"

namespace PCGActorAndComponentMapping
{
	FBox GetActorBounds(const AActor* InActor)
	{
		FBox ActorBounds = InActor->GetComponentsBoundingBox();
		if (!ActorBounds.IsValid && InActor->GetRootComponent() != nullptr)
		{
			// Try on the RootComponent
			ActorBounds = InActor->GetRootComponent()->Bounds.GetBox();
		}

		return ActorBounds;
	}

#if WITH_EDITOR
	static TAutoConsoleVariable<bool> CVarDisableObjectDependenciesTracking(
		TEXT("pcg.DisableObjectDependenciesTracking"),
		false,
		TEXT("If depencencies are being unstable, disable the tracking, allowing people to continue working while we investigate."));

	static TAutoConsoleVariable<bool> CVarDisableDelayedActorRegistering(
		TEXT("pcg.DisableDelayedActorRegistering"),
		false,
		TEXT("If delayed actor registering when their components aren't registered yet is introducing bad behavior, disables it, allowing people to continue working while we investigate."));

	static TAutoConsoleVariable<bool> CVarLandscapeDisableRefreshTracking(
		TEXT("pcg.LandscapeDisableRefreshTracking"),
		false,
		TEXT("Completely disable landscape refresh when it changes."));

	static TAutoConsoleVariable<bool> CVarLandscapeDisableRefreshTrackingInLandscapeEditingMode(
		TEXT("pcg.LandscapeDisableRefreshTrackingInLandscapeEditingMode"),
		false,
		TEXT("Disable landscape refresh when it changes in landscape editing mode."));

	static TAutoConsoleVariable<int> CVarLandscapeRefreshTimeDelay(
		TEXT("pcg.LandscapeRefreshTimeDelayMS"),
		1000,
		TEXT("Time in MS between a landscape change and PCG refresh. Set it to 0 or negative value to disable the delay."));
#endif // WITH_EDITOR

	static TAutoConsoleVariable<bool> CVarDisableDelayedUnregister(
		TEXT("pcg.DisableDelayedUnregister"),
		true,
		TEXT("If delayed unregister for all is introducing bad behavior, disables it, allowing people to continue working while we investigate."));

#if WITH_EDITOR
	void PropagateToLevelInstanceActors(AActor* InActor, UPCGSubsystem* PCGSubsystem, TFunctionRef<bool(AActor* LevelActor)> InFunc)
	{
		if (!PCGSubsystem || !PCGSubsystem->GetWorld())
		{
			return;
		}

		if (ILevelInstanceInterface* LevelInstance = Cast<ILevelInstanceInterface>(InActor))
		{
			if (const ULevelInstanceSubsystem* LevelInstanceSubsystem = PCGSubsystem->GetWorld()->GetSubsystem<ULevelInstanceSubsystem>())
			{
				LevelInstanceSubsystem->ForEachActorInLevelInstance(LevelInstance, InFunc);
			}
		}
	}

	bool ShouldDiscardLandscapeRefresh(const ALandscapeProxy* InLandscape, bool& bIsInEditingMode, bool bIsExitingEditingMode)
	{
		bIsInEditingMode = false;
		// If it is not a landscape, we should refresh.
		if (!InLandscape)
		{
			return false;
		}

		// If refresh is globably disabled, never refresh
		if (PCGActorAndComponentMapping::CVarLandscapeDisableRefreshTracking.GetValueOnAnyThread())
		{
			return true;
		}

		// If refresh is not disabled in editing, always refresh
		if (!PCGActorAndComponentMapping::CVarLandscapeDisableRefreshTrackingInLandscapeEditingMode.GetValueOnAnyThread())
		{
			return false;
		}

		// Refresh only if we are not editing.
		const ALandscape* Landscape = InLandscape->GetLandscapeActor();
		bIsInEditingMode = Landscape && Landscape->HasLandscapeEdMode() && !bIsExitingEditingMode;
		return bIsInEditingMode;
	}
#endif
}

FPCGActorAndComponentMapping::FPCGActorAndComponentMapping(UPCGSubsystem* InPCGSubsystem)
	: PCGSubsystem(InPCGSubsystem)
{
	check(PCGSubsystem);

	// TODO: For now we set our octree to be 2km wide, but it would be perhaps better to
	// scale it to the size of our world.
	constexpr FVector::FReal OctreeExtent = 200000; // 2km
	PartitionedOctree.Reset(FVector::ZeroVector, OctreeExtent);
	NonPartitionedOctree.Reset(FVector::ZeroVector, OctreeExtent);
}

void FPCGActorAndComponentMapping::Tick()
{
	TSet<UPCGComponent*> ComponentToUnregister;
	{
		FScopeLock Lock(&DelayedComponentToUnregisterLock);
		ComponentToUnregister = MoveTemp(DelayedComponentToUnregister);
	}

	for (UPCGComponent* Component : ComponentToUnregister)
	{
		UnregisterPCGComponent(Component, /*bForce=*/true);
	}

#if WITH_EDITOR
	AddDelayedActors();

	if (!DelayedModifiedLandscapes.IsEmpty() && LastLandscapeDirtyTime > 0.0 && ((FApp::GetCurrentTime() - LastLandscapeDirtyTime) * 1000.0) > PCGActorAndComponentMapping::CVarLandscapeRefreshTimeDelay.GetValueOnAnyThread())
	{
		LastLandscapeDirtyTime = -1.0;
		for (TObjectKey<ALandscapeProxy> Landscape : DelayedModifiedLandscapes)
		{
			ApplyLandscapeChanges(Landscape.ResolveObjectPtr());
		}

		DelayedModifiedLandscapes.Empty();
	}
#endif // WITH_EDITOR
}

TArray<FPCGTaskId> FPCGActorAndComponentMapping::DispatchToRegisteredLocalComponents(UPCGComponent* OriginalComponent, const TFunction<FPCGTaskId(UPCGComponent*)>& InFunc) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGActorAndComponentMapping::DispatchToRegisteredLocalComponents);
	if (!ensure(OriginalComponent))
	{
		return {};
	}

	const bool bIsRuntimeGenerated = OriginalComponent->IsManagedByRuntimeGenSystem();

	const TMap<const UPCGComponent*, TSet<TObjectPtr<APCGPartitionActor>>>& Map = bIsRuntimeGenerated ? ComponentToRuntimeGenPartitionActorsMap : ComponentToPartitionActorsMap;

	// TODO: Might be more interesting to copy the set and release the lock.
	FReadScopeLock ReadLock(bIsRuntimeGenerated ? ComponentToRuntimeGenPartitionActorsMapLock : ComponentToPartitionActorsMapLock);
	const TSet<TObjectPtr<APCGPartitionActor>>* PartitionActorsPtr = Map.Find(OriginalComponent);

	if (!PartitionActorsPtr)
	{
		return TArray<FPCGTaskId>();
	}

	return DispatchToLocalComponents(OriginalComponent, *PartitionActorsPtr, InFunc);
}

TArray<FPCGTaskId> FPCGActorAndComponentMapping::DispatchToLocalComponents(UPCGComponent* OriginalComponent, const TSet<TObjectPtr<APCGPartitionActor>>& PartitionActors, const TFunction<FPCGTaskId(UPCGComponent*)>& InFunc) const
{
	TArray<FPCGTaskId> TaskIds;
	for (APCGPartitionActor* PartitionActor : PartitionActors)
	{
		if (PartitionActor)
		{
			if (UPCGComponent* LocalComponent = PartitionActor->GetLocalComponent(OriginalComponent))
			{
				// Add check to avoid infinite loop
				if (ensure(!LocalComponent->IsPartitioned()))
				{
					FPCGTaskId LocalTask = InFunc(LocalComponent);

					if (LocalTask != InvalidPCGTaskId)
					{
						TaskIds.Add(LocalTask);
					}
				}
			}
		}
	}

	return TaskIds;
}

bool FPCGActorAndComponentMapping::RegisterOrUpdatePCGComponent(UPCGComponent* InComponent, bool bDoActorMapping)
{
	check(InComponent);

	// Discard BP templates, local components and invalid component
	if (!IsValid(InComponent) || !InComponent->GetOwner() || InComponent->GetOwner()->IsA<APCGPartitionActor>())
	{
		return false;
	}

	// Check also that the bounds are valid. If not early out.
	if (!InComponent->GetGridBounds().IsValid)
	{
		UE_LOG(LogPCG, Error, TEXT("[RegisterOrUpdatePCGComponent] Component has invalid bounds, not registered nor updated."));
		return false;
	}

	const bool bWasAlreadyRegistered = IsComponentRegistered(InComponent);

	// First check if the component has changed its partitioned flag.
	const bool bIsPartitioned = InComponent->IsPartitioned();
	if (bIsPartitioned && NonPartitionedOctree.Contains(InComponent))
	{
		UnregisterNonPartitionedPCGComponent(InComponent);
	}
	else if (!bIsPartitioned && PartitionedOctree.Contains(InComponent))
	{
		UnregisterPartitionedPCGComponent(InComponent);
	}

	PCGSubsystem->OnOriginalComponentRegistered(InComponent);

	// Then register/update accordingly
	bool bHasChanged = false;
	if (bIsPartitioned)
	{
		bHasChanged = RegisterOrUpdatePartitionedPCGComponent(InComponent, bDoActorMapping);
	}
	else
	{
		bHasChanged = RegisterOrUpdateNonPartitionedPCGComponent(InComponent);
	}

	// If the component was previously marked as to be unregistered, remove it here.
	{
		FScopeLock Lock(&DelayedComponentToUnregisterLock);
		DelayedComponentToUnregister.Remove(InComponent);
	}

	// And finally handle the tracking. Only do it when the component is registered for the first time.
#if WITH_EDITOR
	if (!bWasAlreadyRegistered && bHasChanged)
	{
		RegisterTracking(InComponent);
	}
#endif // WITH_EDITOR

	return bHasChanged;
}

bool FPCGActorAndComponentMapping::RegisterOrUpdatePartitionedPCGComponent(UPCGComponent* InComponent, bool bDoActorMapping)
{
	FBox Bounds(EForceInit::ForceInit);
	bool bComponentHasChanged = false;
	bool bComponentWasAdded = false;

	PartitionedOctree.AddOrUpdateComponent(InComponent, Bounds, bComponentHasChanged, bComponentWasAdded);

#if WITH_EDITOR
	// In Editor only, we will create new partition actors depending on the new bounds and generation trigger. Runtime managed components should not create PAs here
	// TODO: For now it will always create the PA. But if we want to create them only when we generate, we need to make
	// sure to update the runtime flow, for them to also create PA if they need to.
	if ((bComponentHasChanged || bComponentWasAdded) && !InComponent->IsManagedByRuntimeGenSystem())
	{
		bool bHasUnbounded = false;
		PCGHiGenGrid::FSizeArray GridSizes;
		ensure(PCGHelpers::GetGenerationGridSizes(InComponent ? InComponent->GetGraph() : nullptr, PCGSubsystem->GetPCGWorldActor(), GridSizes, bHasUnbounded));
		PCGSubsystem->CreatePartitionActorsWithinBounds(Bounds, GridSizes);
	}
#endif // WITH_EDITOR

	// After adding/updating, try to do the mapping (if we asked for it and the component changed)
	if (bDoActorMapping)
	{
		if (bComponentHasChanged)
		{
			UpdateMappingPCGComponentPartitionActor(InComponent);
		}
	}
	else
	{
		if (!bComponentWasAdded)
		{
			// If we do not want a mapping, delete the existing one
			DeleteMappingPCGComponentPartitionActor(InComponent);
		}
	}

	return bComponentHasChanged;
}

bool FPCGActorAndComponentMapping::RegisterOrUpdateNonPartitionedPCGComponent(UPCGComponent* InComponent)
{
	// Tracking is only done in Editor for now
#if WITH_EDITOR
	FBox Bounds(EForceInit::ForceInit);
	bool bComponentHasChanged = false;
	bool bComponentWasAdded = false;

	NonPartitionedOctree.AddOrUpdateComponent(InComponent, Bounds, bComponentHasChanged, bComponentWasAdded);

	return bComponentHasChanged;
#else
	return false;
#endif // WITH_EDITOR
}

bool FPCGActorAndComponentMapping::RemapPCGComponent(const UPCGComponent* OldComponent, UPCGComponent* NewComponent, bool bDoActorMapping)
{
	check(OldComponent && NewComponent);

	bool bBoundsChanged = false;

	if (OldComponent->IsPartitioned())
	{
		if (!PartitionedOctree.RemapComponent(OldComponent, NewComponent, bBoundsChanged))
		{
			return false;
		}
	}
	else
	{
		if (!NonPartitionedOctree.RemapComponent(OldComponent, NewComponent, bBoundsChanged))
		{
			return false;
		}
	}

	// Remove it from the delayed
	{
		FScopeLock Lock(&DelayedComponentToUnregisterLock);
		DelayedComponentToUnregister.Remove(OldComponent);
	}

	// Remap all previous instances
	auto RemapPreviousInstances = [OldComponent, NewComponent](TMap<const UPCGComponent*, TSet<TObjectPtr<APCGPartitionActor>>>& Map, FRWLock& Lock)
	{
		FWriteScopeLock WriteLock(Lock);

		if (TSet<TObjectPtr<APCGPartitionActor>>* PartitionActorsPtr = Map.Find(OldComponent))
		{
			TSet<TObjectPtr<APCGPartitionActor>> PartitionActorsToRemap = MoveTemp(*PartitionActorsPtr);
			Map.Remove(OldComponent);

			for (APCGPartitionActor* Actor : PartitionActorsToRemap)
			{
				Actor->RemapGraphInstance(OldComponent, NewComponent);
			}

			Map.Add(NewComponent, MoveTemp(PartitionActorsToRemap));
		}
	};

	RemapPreviousInstances(ComponentToPartitionActorsMap, ComponentToPartitionActorsMapLock);
	RemapPreviousInstances(ComponentToRuntimeGenPartitionActorsMap, ComponentToRuntimeGenPartitionActorsMapLock);

	// And update the mapping if bounds changed and we want to do actor mapping
	if (bBoundsChanged && NewComponent->IsPartitioned() && bDoActorMapping)
	{
		UpdateMappingPCGComponentPartitionActor(NewComponent);
	}

#if WITH_EDITOR
	RemapTracking(OldComponent, NewComponent);
#endif // WITH_EDITOR

	return true;
}

void FPCGActorAndComponentMapping::UnregisterPCGComponent(UPCGComponent* InComponent, bool bForce)
{
	if (!InComponent)
	{
		return;
	}

	if ((PartitionedOctree.Contains(InComponent) || NonPartitionedOctree.Contains(InComponent)))
	{
		bool bShouldBeDelayed = true;
		if (PCGActorAndComponentMapping::CVarDisableDelayedUnregister.GetValueOnAnyThread())
		{
			// We also need to check that our current PCG Component is not deleted while being reconstructed by a construction script.
			// If so, it will be "re-created" at some point with the same properties.
			// In this particular case, we don't remove the PCG component from the octree and we won't delete the mapping, but mark it to be removed
			// at next Subsystem tick. If we call "RemapPCGComponent" before, we will re-connect everything correctly.
			// Ignore this if we force (aka when we actually unregister the delayed one)
			bShouldBeDelayed = InComponent->IsCreatedByConstructionScript();
		}

		if (!bForce && bShouldBeDelayed)
		{
			FScopeLock Lock(&DelayedComponentToUnregisterLock);
			DelayedComponentToUnregister.Add(InComponent);
			return;
		}

#if WITH_EDITOR
		UnregisterTracking(InComponent);
#endif // WITH_EDITOR
	}

	UnregisterPartitionedPCGComponent(InComponent);
	UnregisterNonPartitionedPCGComponent(InComponent);

	FScopeLock Lock(&DelayedComponentToUnregisterLock);
	if (DelayedComponentToUnregister.Contains(InComponent))
	{
		DelayedComponentToUnregister.Remove(InComponent);
	}
}

void FPCGActorAndComponentMapping::UnregisterPartitionedPCGComponent(UPCGComponent* InComponent)
{
	PCGSubsystem->OnOriginalComponentUnregistered(InComponent);

	if (!PartitionedOctree.RemoveComponent(InComponent) || InComponent->IsManagedByRuntimeGenSystem())
	{
		return;
	}

	// Because of recursive component deletes actors that has components, we cannot do RemoveGraphInstance
	// inside a lock. So copy the actors to clean up and release the lock before doing RemoveGraphInstance.
	TSet<TObjectPtr<APCGPartitionActor>> PartitionActorsToCleanUp;
	{
		FWriteScopeLock WriteLock(ComponentToPartitionActorsMapLock);
		TSet<TObjectPtr<APCGPartitionActor>>* PartitionActorsPtr = ComponentToPartitionActorsMap.Find(InComponent);

		if (PartitionActorsPtr)
		{
			PartitionActorsToCleanUp = MoveTemp(*PartitionActorsPtr);
			ComponentToPartitionActorsMap.Remove(InComponent);
		}
	}

	for (APCGPartitionActor* Actor : PartitionActorsToCleanUp)
	{
		Actor->RemoveGraphInstance(InComponent);
	}
}

void FPCGActorAndComponentMapping::UnregisterNonPartitionedPCGComponent(UPCGComponent* InComponent)
{
	PCGSubsystem->OnOriginalComponentUnregistered(InComponent);

	NonPartitionedOctree.RemoveComponent(InComponent);
}

void FPCGActorAndComponentMapping::ForAllIntersectingComponents(const FBoxCenterAndExtent& InBounds, TFunction<void(UPCGComponent*)> InFunc) const
{
	PartitionedOctree.FindElementsWithBoundsTest(InBounds, [&InFunc](const FPCGComponentRef& ComponentRef)
	{
		InFunc(ComponentRef.Component);
	});
}

void FPCGActorAndComponentMapping::RegisterPartitionActor(APCGPartitionActor* InActor, bool bDoComponentMapping)
{
	check(InActor);

	const uint32 GridSize = InActor->GetPCGGridSize();
	const FIntVector GridCoord = InActor->GetGridCoord();

	check(GridSize > 0);

	const bool bIsRuntimeGenerated = InActor->IsRuntimeGenerated();
	TMap<uint32, TMap<FIntVector, TObjectPtr<APCGPartitionActor>>>& ActorsMap = bIsRuntimeGenerated ? RuntimeGenPartitionActorsMap : PartitionActorsMap;

	{
		FWriteScopeLock WriteLock(bIsRuntimeGenerated ? RuntimeGenPartitionActorsMapLock : PartitionActorsMapLock);

		TMap<FIntVector, TObjectPtr<APCGPartitionActor>>& PartitionActorsMapGrid = ActorsMap.FindOrAdd(GridSize);
		if (PartitionActorsMapGrid.Contains(GridCoord))
		{
			return;
		}

		PartitionActorsMapGrid.Add(GridCoord, InActor);
	}

	// For deprecration: bUse2DGrid is now true by default. But if we already have Partition Actors that were created when the flag was false by default,
	// we keep this flag
	if (APCGWorldActor* WorldActor = PCGSubsystem->GetPCGWorldActor())
	{
		if (WorldActor->bUse2DGrid != InActor->IsUsing2DGrid())
		{
			WorldActor->bUse2DGrid = InActor->IsUsing2DGrid();
		}
	}

	// Register to all the components that intersect with the PA. Ignore for runtime generated, it is handled manually
	if (!bIsRuntimeGenerated)
	{
		FWriteScopeLock WriteLock(ComponentToPartitionActorsMapLock);
		ForAllIntersectingComponents(FBoxCenterAndExtent(InActor->GetFixedBounds()), [this, InActor, bDoComponentMapping](UPCGComponent* Component)
		{
			// For each component, do the mapping if we ask it explicitly, or if the component is generated
			if (bDoComponentMapping || Component->bGenerated)
			{
				TSet<TObjectPtr<APCGPartitionActor>>* PartitionActorsPtr = ComponentToPartitionActorsMap.Find(Component);
				// In editor we might load/create partition actors while the component is registering. Because of that,
				// the mapping might not already exists, even if the component is marked generated.
				if (PartitionActorsPtr)
				{
					InActor->AddGraphInstance(Component);
					PartitionActorsPtr->Add(InActor);
				}
			}
		});
	}
}

void FPCGActorAndComponentMapping::UnregisterPartitionActor(APCGPartitionActor* Actor)
{
	check(Actor);

	const FIntVector GridCoord = Actor->GetGridCoord();
	const uint32 GridSize = Actor->GetPCGGridSize();
	if (!ensure(GridSize > 0))
	{
		return;
	}

	const bool bIsRuntimeGenerated = Actor->IsRuntimeGenerated();
	TMap<uint32, TMap<FIntVector, TObjectPtr<APCGPartitionActor>>>& ActorsMap = bIsRuntimeGenerated ? RuntimeGenPartitionActorsMap : PartitionActorsMap;

	if (TMap<FIntVector, TObjectPtr<APCGPartitionActor>>* PartitionActorsMapGrid = ActorsMap.Find(GridSize))
	{
		FWriteScopeLock WriteLock(bIsRuntimeGenerated ? RuntimeGenPartitionActorsMapLock : PartitionActorsMapLock);
		PartitionActorsMapGrid->Remove(GridCoord);
	}

	// Unregister from all intersecting components. Ignore for runtime generated, it is handled manually
	if (!bIsRuntimeGenerated)
	{
		FWriteScopeLock WriteLock(ComponentToPartitionActorsMapLock);
		ForAllIntersectingComponents(FBoxCenterAndExtent(Actor->GetFixedBounds()), [this, Actor](UPCGComponent* Component)
		{
			TSet<TObjectPtr<APCGPartitionActor>>* PartitionActorsPtr = ComponentToPartitionActorsMap.Find(Component);
			if (PartitionActorsPtr)
			{
				PartitionActorsPtr->Remove(Actor);
			}
		});
	}
}

void FPCGActorAndComponentMapping::ForAllIntersectingPartitionActors(const FBox& InBounds, TFunction<void(APCGPartitionActor*)> InFunc) const
{
	// No PCGWorldActor just early out. Same for invalid bounds.
	APCGWorldActor* PCGWorldActor = PCGSubsystem->GetPCGWorldActor();

	if (!PCGWorldActor || !InBounds.IsValid)
	{
		return;
	}

	auto ForAllIntersectingPartitionActorsOfGridSize = [InFunc, PCGWorldActor, &InBounds](const TMap<uint32, TMap<FIntVector, TObjectPtr<APCGPartitionActor>>>& Map, FRWLock& Lock, uint32 GridSize)
	{
		const bool bUse2DGrid = PCGWorldActor->bUse2DGrid;
		FIntVector MinCellCoords = UPCGActorHelpers::GetCellCoord(InBounds.Min, GridSize, bUse2DGrid);
		FIntVector MaxCellCoords = UPCGActorHelpers::GetCellCoord(InBounds.Max, GridSize, bUse2DGrid);

		FReadScopeLock ReadLock(Lock);

		const TMap<FIntVector, TObjectPtr<APCGPartitionActor>>* PartitionActorsMapGrid = Map.Find(GridSize);
		if (!PartitionActorsMapGrid || PartitionActorsMapGrid->IsEmpty())
		{
			return;
		}

		for (int32 z = MinCellCoords.Z; z <= MaxCellCoords.Z; z++)
		{
			for (int32 y = MinCellCoords.Y; y <= MaxCellCoords.Y; y++)
			{
				for (int32 x = MinCellCoords.X; x <= MaxCellCoords.X; x++)
				{
					FIntVector CellCoords(x, y, z);
					if (const TObjectPtr<APCGPartitionActor>* ActorPtr = PartitionActorsMapGrid->Find(CellCoords))
					{
						if (APCGPartitionActor* Actor = ActorPtr->Get())
						{
							InFunc(Actor);
						}
					}
				}
			}
		}
	};

	PCGHiGenGrid::FSizeToGuidMap GridSizeToGuid;
	PCGWorldActor->GetSerializedGridGuids(GridSizeToGuid);
	for (const TPair<uint32, FGuid>& SizeAndGuid : GridSizeToGuid)
	{
		const uint32 GridSize = SizeAndGuid.Key;
		ForAllIntersectingPartitionActorsOfGridSize(PartitionActorsMap, PartitionActorsMapLock, GridSize);
	}

	GridSizeToGuid.Empty();
	PCGWorldActor->GetTransientGridGuids(GridSizeToGuid);
	for (const TPair<uint32, FGuid>& SizeAndGuid : GridSizeToGuid)
	{
		const uint32 GridSize = SizeAndGuid.Key;
		ForAllIntersectingPartitionActorsOfGridSize(RuntimeGenPartitionActorsMap, RuntimeGenPartitionActorsMapLock, GridSize);
	}
}

void FPCGActorAndComponentMapping::UpdateMappingPCGComponentPartitionActor(UPCGComponent* InComponent)
{
	if (!PCGSubsystem->IsInitialized())
	{
		return;
	}

	check(InComponent);

	// Get the bounds
	FBox Bounds = PartitionedOctree.GetBounds(InComponent);

	if (!Bounds.IsValid)
	{
		return;
	}

	TSet<TObjectPtr<APCGPartitionActor>> RemovedActors;

	if (const APCGWorldActor* WorldActor = PCGSubsystem->GetPCGWorldActor())
	{
		const bool bIsHiGenEnabled = InComponent->GetGraph() && InComponent->GetGraph()->IsHierarchicalGenerationEnabled();

		auto UpdateMapping = [this, InComponent, &Bounds, &RemovedActors, WorldActor, bIsHiGenEnabled](TMap<const UPCGComponent*, TSet<TObjectPtr<APCGPartitionActor>>>& Map, FRWLock& Lock)
		{
			FWriteScopeLock WriteLock(Lock);
			TSet<TObjectPtr<APCGPartitionActor>>* PartitionActorsPtr = Map.Find(InComponent);

			if (!PartitionActorsPtr)
			{
				// Does not yet exists, add it
				PartitionActorsPtr = &Map.Emplace(InComponent);
				check(PartitionActorsPtr);
			}

			TSet<TObjectPtr<APCGPartitionActor>> NewMapping;
			ForAllIntersectingPartitionActors(Bounds, [&NewMapping, InComponent, WorldActor, bIsHiGenEnabled](APCGPartitionActor* Actor)
			{
				// If this graph does not have HiGen enabled, we should only add a graph instance for
				// the partition actors whose grid size matches the WorldActor's partition grid size
				if (bIsHiGenEnabled || (Actor && Actor->GetPCGGridSize() == WorldActor->PartitionGridSize))
				{
					Actor->AddGraphInstance(InComponent);
					NewMapping.Add(Actor);
				}
			});

			// Find the ones that were removed
			RemovedActors = PartitionActorsPtr->Difference(NewMapping);

			*PartitionActorsPtr = MoveTemp(NewMapping);
		};

		UpdateMapping(ComponentToPartitionActorsMap, ComponentToPartitionActorsMapLock);
		UpdateMapping(ComponentToRuntimeGenPartitionActorsMap, ComponentToRuntimeGenPartitionActorsMapLock);
	}

	// No need to be locked to do this.
	for (APCGPartitionActor* RemovedActor : RemovedActors)
	{
		RemovedActor->RemoveGraphInstance(InComponent);
	}
}

void FPCGActorAndComponentMapping::DeleteMappingPCGComponentPartitionActor(UPCGComponent* InComponent)
{
	check(InComponent);

	if (!InComponent->IsPartitioned())
	{
		return;
	}

	auto DeleteMapping = [this, InComponent](TMap<const UPCGComponent*, TSet<TObjectPtr<APCGPartitionActor>>>& Map, FRWLock& Lock)
	{
		FWriteScopeLock WriteLock(Lock);

		if (TSet<TObjectPtr<APCGPartitionActor>>* PartitionActorsPtr = Map.Find(InComponent))
		{
			for (APCGPartitionActor* Actor : *PartitionActorsPtr)
			{
				Actor->RemoveGraphInstance(InComponent);
			}

			PartitionActorsPtr->Empty();
		}
	};

	DeleteMapping(ComponentToPartitionActorsMap, ComponentToPartitionActorsMapLock);
	DeleteMapping(ComponentToRuntimeGenPartitionActorsMap, ComponentToRuntimeGenPartitionActorsMapLock);
}

bool FPCGActorAndComponentMapping::IsComponentRegistered(const UPCGComponent* InComponent) const
{
	return PartitionedOctree.Contains(InComponent) || NonPartitionedOctree.Contains(InComponent);
}

bool FPCGActorAndComponentMapping::AnyRuntimeGenComponentsExist() const
{
	for (UPCGComponent* Component : PartitionedOctree.GetAllComponents())
	{
		if (Component && Component->IsManagedByRuntimeGenSystem())
		{
			return true;
		}
	}

	for (UPCGComponent* Component : NonPartitionedOctree.GetAllComponents())
	{
		if (Component && Component->IsManagedByRuntimeGenSystem())
		{
			return true;
		}
	}

	return false;
}

TSet<UPCGComponent*> FPCGActorAndComponentMapping::GetAllRegisteredPartitionedComponents() const
{
	return PartitionedOctree.GetAllComponents();
}

TSet<UPCGComponent*> FPCGActorAndComponentMapping::GetAllRegisteredNonPartitionedComponents() const
{
	return NonPartitionedOctree.GetAllComponents();
}

TSet<UPCGComponent*> FPCGActorAndComponentMapping::GetAllRegisteredComponents() const
{
	TSet<UPCGComponent*> Res = GetAllRegisteredPartitionedComponents();
	Res.Append(GetAllRegisteredNonPartitionedComponents());
	return Res;
}

UPCGComponent* FPCGActorAndComponentMapping::GetLocalComponent(uint32 GridSize, const FIntVector& CellCoords, const UPCGComponent* InOriginalComponent, bool bRuntimeGenerated)
{
	TMap<uint32, TMap<FIntVector, TObjectPtr<APCGPartitionActor>>>& ActorsMap = bRuntimeGenerated ? RuntimeGenPartitionActorsMap : PartitionActorsMap;
	FReadScopeLock ReadLock(bRuntimeGenerated ? RuntimeGenPartitionActorsMapLock : PartitionActorsMapLock);

	if (const TMap<FIntVector, TObjectPtr<APCGPartitionActor>>* PartitionActorsOnGrid = ActorsMap.Find(GridSize))
	{
		const TObjectPtr<APCGPartitionActor>* PartitionActor = PartitionActorsOnGrid->Find(CellCoords);
		if (PartitionActor && *PartitionActor)
		{
			TArray<UPCGComponent*, TInlineAllocator<4>> PCGComponents;
			(*PartitionActor)->GetComponents(PCGComponents);

			UPCGComponent** MatchingComponent = PCGComponents.FindByPredicate([InOriginalComponent](UPCGComponent* Comp)
			{
				return Comp->GetOriginalComponent() == InOriginalComponent;
			});

			return MatchingComponent ? *MatchingComponent : nullptr;
		}
	}

	return nullptr;
}

APCGPartitionActor* FPCGActorAndComponentMapping::GetPartitionActor(uint32 GridSize, const FIntVector& CellCoords, bool bRuntimeGenerated) const
{
	const TMap<uint32, TMap<FIntVector, TObjectPtr<APCGPartitionActor>>>& Map = bRuntimeGenerated ? RuntimeGenPartitionActorsMap : PartitionActorsMap;
	FReadScopeLock ReadLock(bRuntimeGenerated ? RuntimeGenPartitionActorsMapLock : PartitionActorsMapLock);

	if (const TMap<FIntVector, TObjectPtr<APCGPartitionActor>>* PartitionActorsOnGrid = Map.Find(GridSize))
	{
		const TObjectPtr<APCGPartitionActor>* PartitionActor = PartitionActorsOnGrid->Find(CellCoords);
		return PartitionActor ? *PartitionActor : nullptr;
	}

	return nullptr;
}

#if WITH_EDITOR
void FPCGActorAndComponentMapping::RegisterTracking(UPCGComponent* InComponent)
{
	// Discard BP templates, local components and invalid component
	if (!IsValid(InComponent) || !InComponent->GetOwner() || InComponent->GetOwner()->IsA<APCGPartitionActor>())
	{
		return;
	}

	AActor* ComponentOwner = InComponent->GetOwner();

	// If we have no owner, we might be in a BP so don't track
	if (!ComponentOwner)
	{
		return;
	}

	UWorld* World = PCGSubsystem ? PCGSubsystem->GetWorld() : nullptr;

	if (!World)
	{
		return;
	}

	// Components owner needs to be always tracked
	RegisterActor(ComponentOwner);
	AlwaysTrackedActorsToComponentsMap.FindOrAdd(ComponentOwner).Add(InComponent);

	UpdateTracking(InComponent, /*bInShouldDirtyActors=*/ false);

	// Add tracking for when the graph was generated/cleaned, only once
	if (!InComponent->OnPCGGraphGeneratedDelegate.IsBoundToObject(this))
	{
		InComponent->OnPCGGraphGeneratedDelegate.AddRaw(this, &FPCGActorAndComponentMapping::OnPCGGraphGeneratedOrCleaned);
		InComponent->OnPCGGraphCleanedDelegate.AddRaw(this, &FPCGActorAndComponentMapping::OnPCGGraphGeneratedOrCleaned);
	}
}

void FPCGActorAndComponentMapping::UpdateTracking(UPCGComponent* InComponent, bool bInShouldDirtyActors, const TArray<FPCGSelectionKey>* ChangedKeys)
{
	// Discard BP templates, local components and invalid component
	if (!IsValid(InComponent) || !InComponent->GetOwner() || InComponent->GetOwner()->IsA<APCGPartitionActor>())
	{
		return;
	}

	AActor* ComponentOwner = InComponent->GetOwner();

	// If we have no owner, we might be in a BP so don't track
	if (!ComponentOwner)
	{
		return;
	}

	UWorld* World = PCGSubsystem ? PCGSubsystem->GetWorld() : nullptr;

	if (!World)
	{
		return;
	}

	// If no keys are provided, update all tracking keys.
	TArray<FPCGSelectionKey> AllKeys;
	if (ChangedKeys == nullptr)
	{
		InComponent->CachedTrackedKeysToSettings.GenerateKeyArray(AllKeys);
		ChangedKeys = &AllKeys;
	}

	check(ChangedKeys);

	if (ChangedKeys->IsEmpty())
	{
		// Nothing to do
		return;
	}

	// And we also need to find all actors that should be tracked
	TSet<TSoftObjectPtr<UObject>> CandidatesForTracking;
	TSet<TSoftObjectPtr<UObject>> CandidatesForUntracking;

	auto GatherObjects = [this, InComponent, &CandidatesForTracking, &CandidatesForUntracking](const FPCGSelectionKey& InKey, bool bInShouldUntrack)
	{
		if (InKey.Selection == EPCGActorSelection::ByPath)
		{
			// Don't track null objects
			if (InKey.ObjectPath.IsNull())
			{
				return;
			}

			TSoftObjectPtr<UObject> Object(InKey.ObjectPath);
			if (bInShouldUntrack && !CandidatesForTracking.Contains(Object))
			{
				CandidatesForUntracking.Add(Object);
			}
			else
			{
				CandidatesForTracking.Add(Object);
				CandidatesForUntracking.Remove(Object);
			}

			return;
		}

		// InKey provide the info for selecting a given actor.
		// We reconstruct the selector settings from this key, and we also force it to SelectMultiple, since
		// we want to gather all the actors that matches this given key.
		FPCGActorSelectorSettings SelectorSettings = FPCGActorSelectorSettings::ReconstructFromKey(InKey);
		SelectorSettings.bSelectMultiple = true;
		TArray<AActor*> AllActors = PCGActorSelector::FindActors(SelectorSettings, InComponent, [](const AActor*) { return true; }, [](const AActor*) { return true; });

		for (AActor* Actor : AllActors)
		{
			if (!Actor)
			{
				continue;
			}

			if (bInShouldUntrack && !CandidatesForTracking.Contains(Actor))
			{
				CandidatesForUntracking.Add(Actor);
			}
			else
			{
				CandidatesForTracking.Add(Actor);
				CandidatesForUntracking.Remove(Actor);
			}
		}
	};

	for (const FPCGSelectionKey& Key : *ChangedKeys)
	{
		const bool bShouldUntrack = !InComponent->CachedTrackedKeysToSettings.Contains(Key);
		GatherObjects(Key, bShouldUntrack);
	}

	const bool bDisableDelayedActorRegistering = PCGActorAndComponentMapping::CVarDisableDelayedActorRegistering.GetValueOnAnyThread();
	for (const TSoftObjectPtr<UObject>& ObjectPtr : CandidatesForTracking)
	{
		AActor* Actor = Cast<AActor>(ObjectPtr.Get());

		if (Actor && !Actor->HasActorRegisteredAllComponents() && !bDisableDelayedActorRegistering)
		{
			DelayedAddedActors.Emplace(Actor, { false, 0 });
			continue;
		}

		bool bShouldCull = false;
		if (!InComponent->IsObjectTracked(ObjectPtr, bShouldCull))
		{
			continue;
		}

		if (Actor)
		{
			// Making sure that we only have the component in one map.
			auto RemoveFromMap = [InComponent, Actor](TMap<TObjectKey<AActor>, TSet<UPCGComponent*>>& InMap)
			{
				if (TSet<UPCGComponent*>* Components = InMap.Find(Actor))
				{
					Components->Remove(InComponent);
					if (Components->IsEmpty())
					{
						InMap.Remove(Actor);
					}
				}
			};

			if (bShouldCull)
			{
				CulledTrackedActorsToComponentsMap.FindOrAdd(Actor).Add(InComponent);
				RemoveFromMap(AlwaysTrackedActorsToComponentsMap);
			}
			else
			{
				AlwaysTrackedActorsToComponentsMap.FindOrAdd(Actor).Add(InComponent);
				RemoveFromMap(CulledTrackedActorsToComponentsMap);
			}

			RegisterActor(Actor);
		}
		else
		{
			TrackedObjectsToComponentsMap.FindOrAdd(ObjectPtr).Add(InComponent);
		}
	}

	// Also unregister all keys that are not tracked anymore.
	UnregisterTracking(InComponent, &CandidatesForUntracking);
}

void FPCGActorAndComponentMapping::RemapTracking(const UPCGComponent* InOldComponent, UPCGComponent* InNewComponent)
{
	auto ReplaceInMap = [InOldComponent, InNewComponent]<typename Key>(TMap<Key, TSet<UPCGComponent*>>& InMap)
	{
		for (TPair<Key, TSet<UPCGComponent*>>& It : InMap)
		{
			if (It.Value.Remove(InOldComponent) > 0)
			{
				It.Value.Add(InNewComponent);
			}
		}
	};

	ReplaceInMap(CulledTrackedActorsToComponentsMap);
	ReplaceInMap(AlwaysTrackedActorsToComponentsMap);
	ReplaceInMap(TrackedObjectsToComponentsMap);

	// Old component will probably die, but we'll force removing the delegates even if it is const.
	if (UPCGComponent* MutableOldComponent = const_cast<UPCGComponent*>(InOldComponent))
	{
		MutableOldComponent->OnPCGGraphGeneratedDelegate.RemoveAll(this);
		MutableOldComponent->OnPCGGraphCleanedDelegate.RemoveAll(this);
	}

	// And just making sure we are not registering multiple times
	if (!InNewComponent->OnPCGGraphGeneratedDelegate.IsBoundToObject(this))
	{
		InNewComponent->OnPCGGraphGeneratedDelegate.AddRaw(this, &FPCGActorAndComponentMapping::OnPCGGraphGeneratedOrCleaned);
		InNewComponent->OnPCGGraphCleanedDelegate.AddRaw(this, &FPCGActorAndComponentMapping::OnPCGGraphGeneratedOrCleaned);
	}
}

void FPCGActorAndComponentMapping::UnregisterTracking(UPCGComponent* InComponent, const TSet<TSoftObjectPtr<UObject>>* OptionalObjectsToUntrack)
{
	if (!InComponent || (OptionalObjectsToUntrack && OptionalObjectsToUntrack->IsEmpty()))
	{
		return;
	}

	TSet<AActor*> ActorCandidatesForUntrack;

	if (OptionalObjectsToUntrack)
	{
		auto RemoveFromMap = [InComponent](auto& InMap, auto& Key, auto OnEmpty)
		{
			if (TSet<UPCGComponent*>* It = InMap.Find(Key))
			{
				It->Remove(InComponent);
				if (It->IsEmpty())
				{
					OnEmpty(Key);
				}
			}
		};

		for (const TSoftObjectPtr<UObject>& ObjectPtr : *OptionalObjectsToUntrack)
		{
			if (AActor* Actor = Cast<AActor>(ObjectPtr.Get()))
			{
				RemoveFromMap(CulledTrackedActorsToComponentsMap, Actor, [&ActorCandidatesForUntrack](AActor* Actor) { ActorCandidatesForUntrack.Add(Actor); });
				RemoveFromMap(AlwaysTrackedActorsToComponentsMap, Actor, [&ActorCandidatesForUntrack](AActor* Actor) { ActorCandidatesForUntrack.Add(Actor); });
			}
			else
			{
				RemoveFromMap(TrackedObjectsToComponentsMap, ObjectPtr, [this](const TSoftObjectPtr<UObject>& ObjectPtr) { UnregisterObject(ObjectPtr); });
			}
		}
	}
	else
	{
		auto RemoveAllFromMap = [InComponent](auto& InMap, auto OnEmpty)
		{
			for (auto& It : InMap)
			{
				It.Value.Remove(InComponent);
				if (It.Value.IsEmpty())
				{
					OnEmpty(It.Key);
				}
			}
		};

		RemoveAllFromMap(CulledTrackedActorsToComponentsMap, [&ActorCandidatesForUntrack](const TObjectKey<AActor>& ActorKey) { ActorCandidatesForUntrack.Add(ActorKey.ResolveObjectPtr()); });
		RemoveAllFromMap(AlwaysTrackedActorsToComponentsMap, [&ActorCandidatesForUntrack](const TObjectKey<AActor>& ActorKey) { ActorCandidatesForUntrack.Add(ActorKey.ResolveObjectPtr()); });
		RemoveAllFromMap(TrackedObjectsToComponentsMap, [this](const TSoftObjectPtr<UObject>& ObjectPtr) { UnregisterObject(ObjectPtr); });
	}

	// We also need to untrack actors that doesn't have any component that tracks them.
	auto ShouldBeRemoved = [](const AActor* InActor, TMap<TObjectKey<AActor>, TSet<UPCGComponent*>>& InMap)
	{
		TSet<UPCGComponent*>* RegisteredComponents = InMap.Find(InActor);
		return !RegisteredComponents || RegisteredComponents->IsEmpty();
	};

	for (AActor* Candidate : ActorCandidatesForUntrack)
	{
		if (!Candidate)
		{
			continue;
		}

		if (Candidate && ShouldBeRemoved(Candidate, AlwaysTrackedActorsToComponentsMap) && ShouldBeRemoved(Candidate, CulledTrackedActorsToComponentsMap))
		{
			UnregisterActor(Candidate);
		}
	}
}

void FPCGActorAndComponentMapping::UnregisterTracking(UPCGComponent* InComponent)
{
	if (!InComponent)
	{
		return;
	}

	UnregisterTracking(InComponent, nullptr);

	InComponent->OnPCGGraphGeneratedDelegate.RemoveAll(this);
	InComponent->OnPCGGraphCleanedDelegate.RemoveAll(this);
}

void FPCGActorAndComponentMapping::ResetPartitionActorsMap()
{
	PartitionActorsMapLock.WriteLock();
	PartitionActorsMap.Empty();
	PartitionActorsMapLock.WriteUnlock();
}

void FPCGActorAndComponentMapping::RegisterTrackingCallbacks()
{
	GEngine->OnActorMoved().AddRaw(this, &FPCGActorAndComponentMapping::OnActorMoved);
	GEngine->OnLevelActorAdded().AddRaw(this, &FPCGActorAndComponentMapping::OnActorAdded);
	GEngine->OnLevelActorDeleted().AddRaw(this, &FPCGActorAndComponentMapping::OnActorDeleted);
	FCoreUObjectDelegates::OnObjectPropertyChanged.AddRaw(this, &FPCGActorAndComponentMapping::OnObjectPropertyChanged);
	FCoreUObjectDelegates::OnPreObjectPropertyChanged.AddRaw(this, &FPCGActorAndComponentMapping::OnPreObjectPropertyChanged);
	FCoreUObjectDelegates::OnObjectPreSave.AddRaw(this, &FPCGActorAndComponentMapping::OnObjectSaved);

	UWorld* World = PCGSubsystem ? PCGSubsystem->GetWorld() : nullptr;
	// Need the World condition for static analysis...
	if (World && IsValid(World) && World->PersistentLevel)
	{
		World->PersistentLevel->OnLoadedActorAddedToLevelEvent.AddRaw(this, &FPCGActorAndComponentMapping::OnActorLoaded);
		World->PersistentLevel->OnLoadedActorRemovedFromLevelEvent.AddRaw(this, &FPCGActorAndComponentMapping::OnActorUnloaded);
	}
}

void FPCGActorAndComponentMapping::TeardownTrackingCallbacks()
{
	GEngine->OnActorMoved().RemoveAll(this);
	GEngine->OnLevelActorAdded().RemoveAll(this);
	GEngine->OnLevelActorDeleted().RemoveAll(this);
	FCoreUObjectDelegates::OnObjectPropertyChanged.RemoveAll(this);
	FCoreUObjectDelegates::OnPreObjectPropertyChanged.RemoveAll(this);
	FCoreUObjectDelegates::OnObjectPreSave.RemoveAll(this);

	UWorld* World = PCGSubsystem ? PCGSubsystem->GetWorld() : nullptr;
	// Need the World condition for static analysis...
	if (World && IsValid(World) && World->PersistentLevel)
	{
		World->PersistentLevel->OnLoadedActorAddedToLevelEvent.RemoveAll(this);
		World->PersistentLevel->OnLoadedActorRemovedFromLevelEvent.RemoveAll(this);
	}
}

void FPCGActorAndComponentMapping::AddDelayedActors()
{
	// Safeguard, we can't add delayed actors if the subsystem is not initialized
	if (!PCGSubsystem || !PCGSubsystem->IsInitialized() || DelayedAddedActors.IsEmpty())
	{
		return;
	}

	TMap<TObjectKey<AActor>, TTuple<bool, int>> StillDelayedActors;
	const bool bDisableDelayedActorRegistering = PCGActorAndComponentMapping::CVarDisableDelayedActorRegistering.GetValueOnAnyThread();

	for (const TPair<TObjectKey<AActor>, TTuple<bool, int>>& ActorPtrAndShouldDirty : DelayedAddedActors)
	{
		AActor* Actor = ActorPtrAndShouldDirty.Key.ResolveObjectPtr();
		if (!Actor)
		{
			continue;
		}

		if (!Actor->HasActorRegisteredAllComponents() && !bDisableDelayedActorRegistering)
		{
			StillDelayedActors.Add(ActorPtrAndShouldDirty);
		}
		else
		{
			// Implementation note: since the delayed actors list is built from top-level actors (e.g. directly in the level) then depth here is 0.
			OnActorAdded_Internal(Actor, ActorPtrAndShouldDirty.Value.Get<0>(), ActorPtrAndShouldDirty.Value.Get<1>(), /*bForceAddDelayedActor=*/true);
		}
	}

	DelayedAddedActors = MoveTemp(StillDelayedActors);
}

void FPCGActorAndComponentMapping::OnActorLoaded(AActor& InActor)
{
	// We have to make sure to not create a infinite loop
	if (InActor.IsA<APCGWorldActor>() || !PCGSubsystem || InActor.GetWorld() != PCGSubsystem->GetWorld())
	{
		return;
	}

	// Loaded actors should not dirty.
	OnActorAdded_Internal(&InActor, /*bShouldDirty=*/ false, /*LevelInstanceDepth=*/ 0);
}

void FPCGActorAndComponentMapping::OnActorAdded(AActor* InActor)
{
	// We have to make sure to not create a infinite loop
	if (!InActor || InActor->IsA<APCGWorldActor>() || !PCGSubsystem || InActor->GetWorld() != PCGSubsystem->GetWorld())
	{
		return;
	}

	int LevelInstanceDepth = 0;

#if WITH_EDITOR
	if (InActor->bIsEditorPreviewActor)
	{
		return;
	}

	if (ALevelInstanceEditorInstanceActor* LevelInstanceEditorInstance = Cast<ALevelInstanceEditorInstanceActor>(InActor))
	{
		const ULevelInstanceSubsystem* LevelInstanceSubsystem = PCGSubsystem->GetWorld() ? PCGSubsystem->GetWorld()->GetSubsystem<ULevelInstanceSubsystem>() : nullptr;
		const ILevelInstanceInterface* ActorLevelInstance = LevelInstanceSubsystem ? LevelInstanceSubsystem->GetParentLevelInstance(InActor) : nullptr;
		while (ActorLevelInstance)
		{
			++LevelInstanceDepth;
			ActorLevelInstance = LevelInstanceSubsystem->GetParentLevelInstance(CastChecked<AActor>(ActorLevelInstance));
		}
	}
#endif // WITH_EDITOR

	// Implementation note: since this is called only for actors directly in the current level, the depth here is 0.
	// Another implementation note: We delay adding because OnActorAdded fires before an actor's properties are set,
	// so the actor is not ready for processing until the next tick.
	DelayedAddedActors.Emplace(InActor, { true, LevelInstanceDepth });
}

void FPCGActorAndComponentMapping::OnActorAdded_Internal(AActor* InActor, bool bShouldDirty, int32 LevelInstanceDepth, bool bForceAddDelayedActor)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGActorAndComponentMapping::OnActorAdded);
	check(InActor && !InActor->IsA<APCGWorldActor>() && PCGSubsystem && InActor->GetWorld() == PCGSubsystem->GetWorld());

	// A delayed actor should only be added from AddDelayedActors(), because that guarantees the new actor has waited at
	// least one tick for its properties to be popualated.
	if (!bForceAddDelayedActor && DelayedAddedActors.Contains(InActor))
	{
		return;
	}

	// If the subsystem is not initialized, wait for it to be, and store all the actors to check
	if (!PCGSubsystem->IsInitialized())
	{
		DelayedAddedActors.Emplace(InActor, { bShouldDirty, LevelInstanceDepth });
		return;
	}

#if WITH_EDITOR
	// When a level instance is added to the level (or loaded), the Level Instance Editor Instance will be spawned
	// but when we get the callback, it is not initialized yet (and does not contain the actors) hence adding it to the delayed actors.
	// Note that while conceptually we'd want a callback when a level instance is loaded, it's a bit tricky vs. what world we want to do our changes in.
	if (ALevelInstanceEditorInstanceActor* LevelInstanceEditorInstance = Cast<ALevelInstanceEditorInstanceActor>(InActor))
	{
		const FLevelInstanceID& LevelInstanceId = LevelInstanceEditorInstance->GetLevelInstanceID();
		const ULevelInstanceSubsystem* LevelInstanceSubsystem = PCGSubsystem->GetWorld() ? PCGSubsystem->GetWorld()->GetSubsystem<ULevelInstanceSubsystem>() : nullptr;

		if (LevelInstanceId.IsValid() && LevelInstanceSubsystem)
		{
			if (ILevelInstanceInterface* LevelInstance = LevelInstanceSubsystem->GetLevelInstance(LevelInstanceId))
			{
				LevelInstanceSubsystem->ForEachActorInLevelInstance(LevelInstance, [this, bShouldDirty, LevelInstanceDepth, LevelInstanceEditorInstance](AActor* LevelActor)
				{
					if (LevelActor && LevelActor != LevelInstanceEditorInstance && !LevelActor->IsA<APCGWorldActor>())
					{
						OnActorAdded_Internal(LevelActor, bShouldDirty, LevelInstanceDepth + 1);
					}
					
					return true;
				});
			}
		}
		else
		{
			DelayedAddedActors.Emplace(InActor, { bShouldDirty, LevelInstanceDepth });
			return;
		}
	}
#endif // WITH_EDITOR

	if (AddOrUpdateTrackedActor(InActor) && bShouldDirty)
	{
		// Finally notify them all
		OnActorChanged(InActor, /*bInHasMoved=*/ false, nullptr, LevelInstanceDepth);
	}
}

bool FPCGActorAndComponentMapping::AddOrUpdateTrackedActor(AActor* InActor)
{
	// We have to make sure to not create a infinite loop
	if (!InActor || InActor->IsA<APCGWorldActor>() || !PCGSubsystem || InActor->GetWorld() != PCGSubsystem->GetWorld())
	{
		return false;
	}

#if WITH_EDITOR
	if (InActor->bIsEditorPreviewActor)
	{
		return false;
	}
#endif

	// Gather all components, and check if they want to track this one
	TSet<UPCGComponent*> AllComponents = GetAllRegisteredComponents();

	TSet<UPCGComponent*>* CulledTrackedComponents = nullptr;
	TSet<UPCGComponent*>* AlwaysTrackedComponents = nullptr;
	
	for (UPCGComponent* PCGComponent : AllComponents)
	{
		// Making sure that they are not currently waiting to die
		{
			FScopeLock Lock(&DelayedComponentToUnregisterLock);
			if (DelayedComponentToUnregister.Contains(PCGComponent))
			{
				continue;
			}
		}

		bool bTrackingIsCulled = false;
		if (PCGComponent && PCGComponent->IsObjectTracked(InActor, bTrackingIsCulled))
		{
			if (bTrackingIsCulled)
			{
				if (!CulledTrackedComponents)
				{
					CulledTrackedComponents = &CulledTrackedActorsToComponentsMap.FindOrAdd(InActor);
				}

				check(CulledTrackedComponents);
				CulledTrackedComponents->Add(PCGComponent);
			}
			else
			{
				if (!AlwaysTrackedComponents)
				{
					AlwaysTrackedComponents = &AlwaysTrackedActorsToComponentsMap.FindOrAdd(InActor);
				}

				check(AlwaysTrackedComponents);
				AlwaysTrackedComponents->Add(PCGComponent);
			}
		}
	}

	if (CulledTrackedComponents || AlwaysTrackedComponents)
	{
		RegisterActor(InActor);
		return true;
	}
	else if (IsActorTracked(InActor))
	{
		// Do some cleanup if the actor was tracked. 
		// We will force the refresh here, so return false to make sure we don't refresh it twice.
		OnActorDeleted(InActor);
		return false;
	}
	else
	{
		// If it is not tracked, and should not be tracked, just do nothing.
		return false;
	}
}

void FPCGActorAndComponentMapping::RegisterActor(AActor* InActor)
{
	if (!InActor)
	{
		return;
	}

	if (ALandscapeProxy* LandscapeProxy = Cast<ALandscapeProxy>(InActor))
	{
		// Only add it once.
		if (!TrackedActorToPositionMap.Contains(InActor))
		{
			LandscapeProxy->OnComponentDataChanged.AddRaw(this, &FPCGActorAndComponentMapping::OnLandscapeChanged);
		}
	}

	TrackedActorToPositionMap.FindOrAdd(InActor) = PCGActorAndComponentMapping::GetActorBounds(InActor);

	// Also gather dependencies
	UpdateActorDependencies(InActor);
}

bool FPCGActorAndComponentMapping::UnregisterActor(AActor* InActor)
{
	if (!InActor)
	{
		return false;
	}

	if (IsActorTracked(InActor))
	{
		TrackedActorToPositionMap.Remove(InActor);
		CulledTrackedActorsToComponentsMap.Remove(InActor);
		AlwaysTrackedActorsToComponentsMap.Remove(InActor);
		TrackedActorsToDependenciesMap.Remove(InActor);

		if (ALandscapeProxy* LandscapeProxy = Cast<ALandscapeProxy>(InActor))
		{
			LandscapeProxy->OnComponentDataChanged.RemoveAll(this);
		}

		return true;
	}
	else
	{
		return false;
	}
}

bool FPCGActorAndComponentMapping::UnregisterObject(const TSoftObjectPtr<UObject>& InObject)
{
	return TrackedObjectsToComponentsMap.Remove(InObject) > 0;
}

void FPCGActorAndComponentMapping::OnActorUnloaded(AActor& InActor)
{
	// Don't dirty on unload (to mirror the behavior in load)
	OnActorDeleted_Internal(&InActor, /*bShouldDirty=*/false, /*LevelInstanceDepth=*/0);
}

void FPCGActorAndComponentMapping::OnActorDeleted(AActor* InActor)
{
	if (!InActor || !PCGSubsystem || InActor->GetWorld() != PCGSubsystem->GetWorld())
	{
		return;
	}

	int LevelInstanceDepth = 0;

#if WITH_EDITOR
	if (InActor->bIsEditorPreviewActor)
	{
		return;
	}

	if (ALevelInstanceEditorInstanceActor* LevelInstanceEditorInstance = Cast<ALevelInstanceEditorInstanceActor>(InActor))
	{
		const ULevelInstanceSubsystem* LevelInstanceSubsystem = PCGSubsystem->GetWorld() ? PCGSubsystem->GetWorld()->GetSubsystem<ULevelInstanceSubsystem>() : nullptr;
		const ILevelInstanceInterface* ActorLevelInstance = LevelInstanceSubsystem ? LevelInstanceSubsystem->GetParentLevelInstance(InActor) : nullptr;
		while (ActorLevelInstance)
		{
			++LevelInstanceDepth;
			ActorLevelInstance = LevelInstanceSubsystem->GetParentLevelInstance(CastChecked<AActor>(ActorLevelInstance));
		}
	}
#endif

	// Implementation note: since this is called only for actors directly in the current level, the depth here is 0.
	OnActorDeleted_Internal(InActor, /*bShouldDirty=*/true, LevelInstanceDepth);
}

void FPCGActorAndComponentMapping::OnActorDeleted_Internal(AActor* InActor, bool bShouldDirty, int32 LevelInstanceDepth)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGActorAndComponentMapping::OnActorDeleted);
	check(InActor && PCGSubsystem && InActor->GetWorld() == PCGSubsystem->GetWorld());

#if WITH_EDITOR
	// When editing/deleting/replacing a level instance, this will allow us to first remove the previously tracked actors that were under the level instance
	if (ALevelInstanceEditorInstanceActor* LevelInstanceEditorInstance = Cast<ALevelInstanceEditorInstanceActor>(InActor))
	{
		const FLevelInstanceID& LevelInstanceId = LevelInstanceEditorInstance->GetLevelInstanceID();
		const ULevelInstanceSubsystem* LevelInstanceSubsystem = PCGSubsystem->GetWorld() ? PCGSubsystem->GetWorld()->GetSubsystem<ULevelInstanceSubsystem>() : nullptr;

		if (LevelInstanceId.IsValid() && LevelInstanceSubsystem)
		{
			if (ILevelInstanceInterface* LevelInstance = LevelInstanceSubsystem->GetLevelInstance(LevelInstanceId))
			{
				LevelInstanceSubsystem->ForEachActorInLevelInstance(LevelInstance, [this, LevelInstanceDepth, LevelInstanceEditorInstance, bShouldDirty](AActor* LevelActor)
				{
					if (LevelActor != LevelInstanceEditorInstance)
					{
						OnActorDeleted_Internal(LevelActor, bShouldDirty, LevelInstanceDepth + 1);
					}

					return true;
				});
			}
		}
	}

	PCGActorAndComponentMapping::PropagateToLevelInstanceActors(InActor, PCGSubsystem, [this, LevelInstanceDepth, bShouldDirty](AActor* LevelActor)
	{
		OnActorDeleted_Internal(LevelActor, bShouldDirty, LevelInstanceDepth + 1);
		return true;
	});
#endif // WITH_EDITOR

	if (!IsActorTracked(InActor))
	{
		return;
	}

	if (bShouldDirty)
	{
		// Notify all components that the actor has changed (was removed), but the Refresh will only happen AFTER the actor was actually removed from the world (because of delayed refresh).
		OnActorChanged(InActor, /*bInHasMoved=*/ false, nullptr, LevelInstanceDepth, /*bNoRefreshOnOwner=*/true);
	}

	// And then delete everything
	UnregisterActor(InActor);
}

void FPCGActorAndComponentMapping::OnActorMoved(AActor* InActor)
{
	OnActorMoved_Internal(InActor, /*LevelInstanceDepth=*/0);
}

void FPCGActorAndComponentMapping::OnActorMoved_Internal(AActor* InActor, int32 LevelInstanceDepth)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGActorAndComponentMapping::OnActorMoved);

	if (!InActor || (PCGSubsystem && InActor->GetWorld() != PCGSubsystem->GetWorld()))
	{
		return;
	}

#if WITH_EDITOR
	if (InActor->bIsEditorPreviewActor)
	{
		return;
	}

	// If we've moved a level instance actor, we instead "push" forward the notification while making sure this will
	//  trigger changes at this "level" in the world hierarchy
	PCGActorAndComponentMapping::PropagateToLevelInstanceActors(InActor, PCGSubsystem, [this, LevelInstanceDepth](AActor* LevelActor)
	{
		OnActorMoved_Internal(LevelActor, LevelInstanceDepth + 1);
		return true;
	});
#endif

	if (!IsActorTracked(InActor))
	{
		return;
	}

	// Notify all components
	OnActorChanged(InActor, /*bInHasMoved=*/ true, nullptr, LevelInstanceDepth);

	// Update Actor position
	if (FBox* ActorBounds = TrackedActorToPositionMap.Find(InActor))
	{
		*ActorBounds = PCGActorAndComponentMapping::GetActorBounds(InActor);
	}
}

void FPCGActorAndComponentMapping::OnPreObjectPropertyChanged(UObject* InObject, const FEditPropertyChain& InEditPropertyChain)
{
	// We want to track tags, to see if a tag was removed
	TempTrackedActorTags.Empty();
	FProperty* MemberProperty = InEditPropertyChain.GetActiveMemberNode() ? InEditPropertyChain.GetActiveMemberNode()->GetValue() : nullptr;
	AActor* Actor = Cast<AActor>(InObject);

	if (!Actor || (PCGSubsystem && Actor->GetWorld() != PCGSubsystem->GetWorld()) || !MemberProperty || MemberProperty->GetFName() != GET_MEMBER_NAME_CHECKED(AActor, Tags))
	{
		return;
	}

#if WITH_EDITOR
	if (Actor->bIsEditorPreviewActor)
	{
		return;
	}
#endif

	TempTrackedActorTags = TSet<FName>(Actor->Tags);
}

void FPCGActorAndComponentMapping::OnObjectSaved(UObject* InObject, FObjectPreSaveContext InObjectSaveContext)
{
	// Only trigger a refresh a new user data and if it is a data table
	// We only track data table because we probably will catch other changes with OnObjectPropertyChanged.
	// To avoid to force the check multiple times (on change + on save)
	if (!InObjectSaveContext.IsProceduralSave() && Cast<UDataTable>(InObject))
	{
		FPropertyChangedEvent Event{ nullptr };
		OnObjectPropertyChanged(InObject, Event);
	}
}

void FPCGActorAndComponentMapping::OnObjectPropertyChanged(UObject* InObject, FPropertyChangedEvent& InEvent)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGActorAndComponentMapping::OnObjectPropertyChanged);

	const bool bValueNotInteractive = (InEvent.ChangeType != EPropertyChangeType::Interactive);
	// Special exception for actor tags, as we can't track otherwise an actor "losing" a tag
	const bool bActorTagChange = (InEvent.Property && InEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(AActor, Tags));
	// Another special exception for texture/mesh compilation. If the InEvent is empty and the object is a texture/mesh, we ignore it.
	const bool bEventIsEmpty = (InEvent.Property == nullptr) && (InEvent.ChangeType == EPropertyChangeType::Unspecified);
	const bool bIsTextureCompilationResult = bEventIsEmpty && InObject && InObject->IsA<UTexture>()
		&& FTextureCompilingManager::Get().IsCompilingTexture(Cast<UTexture>(InObject));
	// There is no equivalent for StaticMesh to know if we are in PostCompilation, so we assume there are still some meshes to compile (including this one).
	// Might be an over-optimistic approach, might need a revisit.
	const bool bIsStaticMeshCompilationResult = bEventIsEmpty && InObject && InObject->IsA<UStaticMesh>()
		&& FStaticMeshCompilingManager::Get().GetNumRemainingMeshes() > 0;

	if ((!bValueNotInteractive && !bActorTagChange) || bIsTextureCompilationResult || bIsStaticMeshCompilationResult)
	{
		return;
	}

	// First check if it is an actor
	AActor* Actor = Cast<AActor>(InObject);

	// Otherwise, if it's an actor component, track it as well
	if (!Actor)
	{
		if (UActorComponent* ActorComponent = Cast<UActorComponent>(InObject))
		{
			Actor = ActorComponent->GetOwner();
		}
	}

	// If we don't find any actor, try to see if it is a dependency
	if (!Actor)
	{
		if (TSet<UPCGComponent*>* Components = TrackedObjectsToComponentsMap.Find(InObject))
		{
			for (UPCGComponent* Component : *Components)
			{
				if (ClearCache(InObject, Component, /*bIntersect=*/ false, {}, InObject))
				{
					Component->DirtyGenerated();
					Component->Refresh();
				}
			}
		}

		for (const TPair<TObjectKey<AActor>, TSet<TObjectPtr<UObject>>>& TrackedActor : TrackedActorsToDependenciesMap)
		{
			if (TrackedActor.Value.Contains(InObject))
			{
				if (AActor* ActorToChange = TrackedActor.Key.ResolveObjectPtr())
				{
					// Ignore property changes on delayed actors. All their properties are still being set.
					if (DelayedAddedActors.Contains(ActorToChange))
					{
						return;
					}

					OnActorChanged(ActorToChange, /*bInHasMoved=*/ false, /*InOriginatingChangeObject=*/ InObject);
					UpdateActorDependencies(ActorToChange);
				}
			}
		}

		return;
	}

	if (PCGSubsystem && Actor->GetWorld() != PCGSubsystem->GetWorld())
	{
		return;
	}

#if WITH_EDITOR
	if (Actor->bIsEditorPreviewActor)
	{
		return;
	}
#endif

	// Ignore property changes on delayed actors. All their properties are still being set.
	if (DelayedAddedActors.Contains(Actor))
	{
		return;
	}

	// Check if we are not tracking it or is a tag change.
	bool bShouldChange = true;
	if (!IsActorTracked(Actor) || bActorTagChange)
	{
		bShouldChange = AddOrUpdateTrackedActor(Actor);
	}

	if (bShouldChange)
	{
		OnActorChanged(Actor, /*bInHasMoved=*/ false, /*InOriginatingChangeObject=*/ InObject);
	}
	else
	{
		// Otherwise we are already tracking the actor, so update its dependencies
		UpdateActorDependencies(Actor);
	}
}

void FPCGActorAndComponentMapping::OnActorChanged(AActor* InActor, bool bInHasMoved, const UObject* InOriginatingChangeObject, int32 LevelInstanceDepth, bool bNoRefreshOwner)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGActorAndComponentMapping::OnActorChanged);

	check(InActor);
	ensure(!PCGSubsystem || InActor->GetWorld() == PCGSubsystem->GetWorld());

	TSet<UPCGComponent*>* CulledTrackedComponents = CulledTrackedActorsToComponentsMap.Find(InActor);
	TSet<UPCGComponent*>* AlwaysTrackedComponents = AlwaysTrackedActorsToComponentsMap.Find(InActor);

	// If this actor is not tracked, just early out
	if (!CulledTrackedComponents && !AlwaysTrackedComponents)
	{
		return;
	}

	TSet<UPCGComponent*> DirtyComponents;

	EPCGComponentDirtyFlag DirtyFlag = EPCGComponentDirtyFlag::Actor;
	if (InActor->IsA<ALandscapeProxy>())
	{
		DirtyFlag = DirtyFlag | EPCGComponentDirtyFlag::Landscape;
	}

	// Check if we have a change of tag too
	TSet<FName> RemovedTags = TempTrackedActorTags.Difference(TSet<FName>(InActor->Tags));

	// We need the actor bounds to know if we are intersecting, for the culled settings to be dirtied.
	const FBox ActorBounds = PCGActorAndComponentMapping::GetActorBounds(InActor);
	// We also need the OldActorBounds if it has moved, and is different than the new bounds.
	FBox* OldActorBoundsPtr = bInHasMoved ? TrackedActorToPositionMap.Find(InActor) : nullptr;
	if (OldActorBoundsPtr && OldActorBoundsPtr->Equals(ActorBounds))
	{
		OldActorBoundsPtr = nullptr;
	}

	if (CulledTrackedComponents)
	{
		// Keep a pointer to the current bounds we are looking at for the partition case. Set it to Actorbounds first, then will be set to old bounds if necessary.
		const FBox* CurrentActorBoundsPtr = &ActorBounds;

		// Then do an octree find to get all components that intersect with this actor.
		// If the actor has moved, we also need to find components that intersected with it before
		// We first do it for non-partitioned, then we do it for partitioned
		auto UpdateNonPartitioned = [this, &DirtyComponents, InActor, CulledTrackedComponents, &RemovedTags, DirtyFlag, InOriginatingChangeObject, bNoRefreshOwner](const FPCGComponentRef& ComponentRef) -> void
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(FPCGActorAndComponentMapping::OnActorChanged::UpdateNonPartitioned);

			// Don't dirty if the component was already dirtied, not tracked, the origin of the change or its owner is the changed actor and we should not refresh.
			if (DirtyComponents.Contains(ComponentRef.Component) || 
				!CulledTrackedComponents->Contains(ComponentRef.Component) ||
				(InOriginatingChangeObject == ComponentRef.Component) ||
				(bNoRefreshOwner && ComponentRef.Component->GetOwner() == InActor))
			{
				return;
			}

			if (ClearCache(InActor, ComponentRef.Component, /*bIntersect=*/true, RemovedTags, InOriginatingChangeObject))
			{
				ComponentRef.Component->DirtyGenerated(DirtyFlag);
				DirtyComponents.Add(ComponentRef.Component);
			}
		};

		NonPartitionedOctree.FindElementsWithBoundsTest(ActorBounds, UpdateNonPartitioned);

		// For partitioned, we first need check if the original component intersect with the bounds, then forward the dirty call only to locals that intersect with the bounds.
		// Note: CurrentActorBoundsPtr is passed by reference because it will be modified between lambda calls (cf comment above).
		auto UpdatePartitioned = [this, &DirtyComponents, InActor, CulledTrackedComponents, &CurrentActorBoundsPtr, &RemovedTags, DirtyFlag, InOriginatingChangeObject, bNoRefreshOwner](const FPCGComponentRef& ComponentRef)  -> void
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(FPCGActorAndComponentMapping::OnActorChanged::UpdatePartitioned);

			// Don't dirty if the component is not tracked, the origin of the change or its owner is the changed actor and we should not refresh.
			// We can "re-dirty" it because changes can impact different local components, from the same
			// original component.
			if (!CulledTrackedComponents->Contains(ComponentRef.Component) ||
				(InOriginatingChangeObject == ComponentRef.Component) ||
				(bNoRefreshOwner && ComponentRef.Component->GetOwner() == InActor))
			{
				return;
			}

			check(CurrentActorBoundsPtr);
			const FBox ComponentBounds = ComponentRef.Bounds.GetBox();
			const bool bIntersect = CurrentActorBoundsPtr->Intersect(ComponentBounds);
			if (!bIntersect)
			{
				return;
			}

			const FBox Overlap = CurrentActorBoundsPtr->Overlap(ComponentBounds);
			bool bWasDirtied = false;

			// Since when we clear the cache for a settings, we clear it all, it's only necessary to do it once on the original component, then only dirty the local that intersects.
			if (ClearCache(InActor, ComponentRef.Component, /*bIntersect=*/true, RemovedTags, InOriginatingChangeObject))
			{
				ForAllIntersectingPartitionActors(Overlap, [InActor, Component = ComponentRef.Component, &bWasDirtied, DirtyFlag, InOriginatingChangeObject](APCGPartitionActor* InPartitionActor) -> void
				{
					if (UPCGComponent* LocalComponent = InPartitionActor->GetLocalComponent(Component))
					{
						bWasDirtied = true;
						LocalComponent->DirtyGenerated(DirtyFlag);
					}
				});
			}

			if (bWasDirtied)
			{
				// Don't dispatch
				ComponentRef.Component->DirtyGenerated(DirtyFlag, /*bDispatchToLocalComponents=*/false);
				DirtyComponents.Add(ComponentRef.Component);
			}
		};

		PartitionedOctree.FindElementsWithBoundsTest(ActorBounds, UpdatePartitioned);

		// If it has moved, redo it with the old bounds.
		if (OldActorBoundsPtr)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(FPCGActorAndComponentMapping::OnActorChanged::SecondUpdateHasMoved);

			// Set the actor bounds with the old one, to have the right Overlap in the Partition case.
			CurrentActorBoundsPtr = OldActorBoundsPtr;
			NonPartitionedOctree.FindElementsWithBoundsTest(*OldActorBoundsPtr, UpdateNonPartitioned);
			PartitionedOctree.FindElementsWithBoundsTest(*OldActorBoundsPtr, UpdatePartitioned);
		}
	}

	// Finally, dirty all components that always track this actor that are not yet notified.
	if (AlwaysTrackedComponents)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FPCGActorAndComponentMapping::OnActorChanged::AlwaysTrackedUpdate);

		for (UPCGComponent* PCGComponent : *AlwaysTrackedComponents)
		{
			if (!PCGComponent || PCGComponent == InOriginatingChangeObject || (bNoRefreshOwner && PCGComponent->GetOwner() == InActor))
			{
				continue;
			}

			// Don't mark "Owner changed" if the change originate from a PCG Component. It will be delegated to the ClearCacheForActor.
			// It is necessary to avoid infine loops when there are multiple PCG components on one actor, and one component was generated.
			const bool bOwnerChanged = (PCGComponent->GetOwner() == InActor) && (!InOriginatingChangeObject || !InOriginatingChangeObject->IsA<UPCGComponent>());
			bool bWasDirtied = false;

			// Since when we clear the cache for a settings, we clear it all, it's only necessary to do it once on the original component. And we will dirty all the local components (with dirty dispatch)
			if (!DirtyComponents.Contains(PCGComponent))
			{
				const FBox ComponentBounds = PCGComponent->GetGridBounds();
				const bool bIntersect = ActorBounds.Intersect(ComponentBounds) || (OldActorBoundsPtr && OldActorBoundsPtr->Intersect(ComponentBounds));
				bWasDirtied = ClearCache(InActor, PCGComponent, /*bIntersect=*/bIntersect, RemovedTags, InOriginatingChangeObject);
			}

			if (bWasDirtied || bOwnerChanged)
			{
				PCGComponent->DirtyGenerated(DirtyFlag, /*bDispatchToLocalComponents=*/true);
				DirtyComponents.Add(PCGComponent);
			}
		}
	}

	// If it is a landscape and we should discard the refresh, early out.
	bool bIsInEditingMode = false;
	ALandscapeProxy* Landscape = Cast<ALandscapeProxy>(InActor);
	if (PCGActorAndComponentMapping::ShouldDiscardLandscapeRefresh(Landscape, bIsInEditingMode, bIsCurrentlyExitingLandscapeEditMode))
	{
		// If we are in editing, keep track of all the dirtied landscape to refresh them when we exit.
		if (bIsInEditingMode)
		{
			DirtiedLandscapes.AddUnique(Landscape);
		}

		return;
	}

	ULevelInstanceSubsystem* LevelInstanceSubsystem = (PCGSubsystem && PCGSubsystem->GetWorld()) ? PCGSubsystem->GetWorld()->GetSubsystem<ULevelInstanceSubsystem>() : nullptr;

	// And refresh all dirtied components
	for (UPCGComponent* Component : DirtyComponents)
	{
		if (!ensure(Component))
		{
			continue;
		}

		const bool bOwnerHasChanged = Component->GetOwner() == InActor;

		if ((!bNoRefreshOwner || !bOwnerHasChanged) && (!Component->bOnlyTrackItself || bOwnerHasChanged))
		{
			// When an object changes, we need to make sure that we don't trigger a refresh on PCG components that are "higher" in the
			// level hierarchy, otherwise we will end up generating in the Level Instance level, which is wrong.
			// Note that in some instances (e.g. when something happens at the Level Instance level) we need to make sure that the
			// PCG components higher-up are properly updated, hence the level instance depth
			if (LevelInstanceSubsystem)
			{
				const ILevelInstanceInterface* ActorLevelInstance = LevelInstanceSubsystem->GetParentLevelInstance(InActor);
				int LocalDepth = LevelInstanceDepth;
				while (ActorLevelInstance && LocalDepth-- > 0)
				{
					ActorLevelInstance = LevelInstanceSubsystem->GetParentLevelInstance(CastChecked<AActor>(ActorLevelInstance));
				}

				const ILevelInstanceInterface* ComponentLevelInstance = LevelInstanceSubsystem->GetParentLevelInstance(Component->GetOwner());
				while (ComponentLevelInstance && ComponentLevelInstance != ActorLevelInstance)
				{
					ComponentLevelInstance = LevelInstanceSubsystem->GetParentLevelInstance(CastChecked<AActor>(ComponentLevelInstance));
				}

				if (ActorLevelInstance != ComponentLevelInstance)
				{
					continue;
				}
			}
			
			Component->Refresh();
		}
	}
}

void FPCGActorAndComponentMapping::OnLandscapeChanged(ALandscapeProxy* InLandscape, const FLandscapeProxyComponentDataChangedParams& InChangeParams)
{
	if (!InLandscape)
	{
		return;
	}

	if (PCGActorAndComponentMapping::CVarLandscapeRefreshTimeDelay.GetValueOnAnyThread() > 0)
	{
		LastLandscapeDirtyTime = FApp::GetCurrentTime();
		DelayedModifiedLandscapes.AddUnique(InLandscape);
	}
	else
	{
		ApplyLandscapeChanges(InLandscape);
	}
}

void FPCGActorAndComponentMapping::ApplyLandscapeChanges(ALandscapeProxy* InLandscape)
{
	if (!InLandscape)
	{
		return;
	}
	
	// We don't know if the landscape moved, only that it has changed. Since `bHasMoved` is doing a bit more, always assume that the landscape has moved.
	OnActorChanged(InLandscape, /*bHasMoved=*/true);

	// Also update its dependencies
	UpdateActorDependencies(InLandscape);
}

void FPCGActorAndComponentMapping::UpdateActorDependencies(AActor* InActor)
{
	if (!InActor)
	{
		return;
	}

	// Don't track what the PCG Component is already tracking (itself and graphs)
	static const TArray<UClass*> ExcludedClasses =
	{
		UPCGComponent::StaticClass(),
		UPCGGraphInterface::StaticClass(),
		UMaterialInterface::StaticClass()
	};


	if (!PCGActorAndComponentMapping::CVarDisableObjectDependenciesTracking.GetValueOnAnyThread())
	{
		TSet<TObjectPtr<UObject>>& DependenciesSet = TrackedActorsToDependenciesMap.FindOrAdd(InActor);
		DependenciesSet.Empty();
		PCGHelpers::GatherDependencies(InActor, DependenciesSet, 1, ExcludedClasses);
	}
}

void FPCGActorAndComponentMapping::OnPCGGraphGeneratedOrCleaned(UPCGComponent* InComponent)
{
	if (!InComponent || !InComponent->GetOwner())
	{
		return;
	}

	OnActorChanged(InComponent->GetOwner(), /*bInHasMoved=*/false, InComponent);
}

bool FPCGActorAndComponentMapping::IsActorTracked(const AActor* InActor) const
{
	return InActor && (CulledTrackedActorsToComponentsMap.Contains(InActor) || AlwaysTrackedActorsToComponentsMap.Contains(InActor));
}

bool FPCGActorAndComponentMapping::ClearCache(const UObject* InObject, const UPCGComponent* InComponent, const bool bIntersect, const TSet<FName>& InRemovedTags, const UObject* InOriginatingChange) const
{
	check(InObject && InComponent);

	if (!PCGSubsystem)
	{
		return false;
	}

	// Special case for the landscape. No settings associated, but we should dirty if the component is tracking the landscape.
	bool bShouldDirty = InComponent->ShouldTrackLandscape() && InObject->IsA<ALandscapeProxy>();

	for (const UPCGSettings* Settings : InComponent->GatherSettingsTracking(InObject, bIntersect, InRemovedTags, InOriginatingChange))
	{
		if (!Settings) 
		{
			continue;
		}

		bShouldDirty = true;
		PCGSubsystem->CleanFromCache(Settings->GetElement().Get(), Settings);
	}

	return bShouldDirty;
}

void FPCGActorAndComponentMapping::NotifyLandscapeEditModeExited()
{
	bIsCurrentlyExitingLandscapeEditMode = true;
	// When the landscape edit mode is exited, force the refresh on all modified/dirtied landscapes.
	for (TObjectKey<ALandscapeProxy> Landscape : DelayedModifiedLandscapes)
	{
		DirtiedLandscapes.AddUnique(Landscape);
	}

	DelayedModifiedLandscapes.Empty();

	for (TObjectKey<ALandscapeProxy> Landscape : DirtiedLandscapes)
	{
		ApplyLandscapeChanges(Landscape.ResolveObjectPtr());
	}

	DirtiedLandscapes.Empty();

	bIsCurrentlyExitingLandscapeEditMode = false;
}
#endif // WITH_EDITOR