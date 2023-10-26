// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartition/HLOD/HLODEditorData.h"

#include "WorldPartition/HLOD/HLODActor.h"
#include "WorldPartition/HLOD/HLODActorDesc.h"
#include "WorldPartition/HLOD/HLODLoaderAdapter.h"

#include "WorldPartition/ActorDescContainerCollection.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionHandle.h"
#include "WorldPartition/WorldPartitionHelpers.h"


FWorldPartitionHLODEditorData::FWorldPartitionHLODEditorData(UWorldPartition* InWorldPartition)
	: WorldPartition(InWorldPartition)
	, LastStateUpdate(INDEX_NONE)
{
	// First pass, build mapping of GUID to HLOD scene node
	for (FActorDescContainerCollection::TIterator<AWorldPartitionHLOD> HLODIterator(WorldPartition); HLODIterator; ++HLODIterator)
	{
		const FHLODActorDesc& HLODActorDesc = **HLODIterator;
		const FGuid& HLODActorGuid = HLODActorDesc.GetGuid();

		TUniquePtr<FHLODSceneNode>& HLODSceneNode = HLODActorNodes.Emplace(HLODActorGuid, new FHLODSceneNode());
		HLODSceneNode->Bounds = HLODActorDesc.GetEditorBounds();
		HLODSceneNode->HLODActorHandle = FWorldPartitionHandle(InWorldPartition, HLODActorDesc.GetGuid());
	}

	// Second pass, build a hierarchy now that nodes were all created
	for (FActorDescContainerCollection::TIterator<AWorldPartitionHLOD> HLODIterator(WorldPartition); HLODIterator; ++HLODIterator)
	{
		const FHLODActorDesc& HLODActorDesc = **HLODIterator;
		const FGuid& HLODActorGuid = HLODActorDesc.GetGuid();
		TUniquePtr<FHLODSceneNode>& HLODSceneNode = HLODActorNodes.FindChecked(HLODActorGuid);

		for (const FGuid& ChildHLODActorGuid : HLODActorDesc.GetChildHLODActors())
		{
			if (TUniquePtr<FHLODSceneNode>* ChildHLODSceneNodePtr = HLODActorNodes.Find(ChildHLODActorGuid))
			{
				FHLODSceneNode* ChildHLODSceneNode = ChildHLODSceneNodePtr->Get();
				ChildHLODSceneNode->ParentHLOD = HLODSceneNode.Get();

				HLODSceneNode->ChildrenHLODs.Add(ChildHLODSceneNode);
			}
		}
	}

	// Cache top level HLOD nodes for a faster iteration during the subsystem tick.
	for (const auto& [HLODActorGuid, HLODActorNode] : HLODActorNodes)
	{
		if (HLODActorNode->ParentHLOD == nullptr)
		{
			TopLevelHLODActorNodes.Add(HLODActorNode.Get());
		}
	}
}

struct FBoundsWithVolume
{
	FBoundsWithVolume(const FBox& InBox)
		: Box(InBox)
		, Volume(Box.GetVolume())
	{
	}

	FBox Box;
	FBox::FReal Volume;
};

// Gather Pinned Actors bounds - also include references & contained actors if the pinned actor is a container.
static void GatherLoadedActorsBounds(TArray<FBoundsWithVolume>& OutLoadedBounds, const FWorldPartitionActorDesc* InActorDesc, const UActorDescContainer* InContainer, const TOptional<FTransform>& InContainerTransform = TOptional<FTransform>())
{
	if (InActorDesc)
	{
		// The actor itself - Include the actor bounds only if HLOD relevant
		if (InActorDesc->GetIsSpatiallyLoaded() && InActorDesc->IsEditorRelevant() && InActorDesc->GetActorIsHLODRelevant())
		{
			const FBox Box = InContainerTransform.IsSet() ? InActorDesc->GetEditorBounds().TransformBy(InContainerTransform.GetValue()) : InActorDesc->GetEditorBounds();
			OutLoadedBounds.Emplace(Box);
		}

		// Test its references
		for (const FGuid& ReferenceGuid : InActorDesc->GetReferences())
		{
			if (const FWorldPartitionActorDesc* ReferenceActorDesc = InContainer->GetActorDesc(ReferenceGuid))
			{
				GatherLoadedActorsBounds(OutLoadedBounds, ReferenceActorDesc, InContainer, InContainerTransform);
			}
		}

		// If it's a container, test the contained actors
		if (InActorDesc->IsContainerInstance())
		{
			FWorldPartitionActorDesc::FContainerInstance ContainerInstance;
			if (InActorDesc->GetContainerInstance(ContainerInstance))
			{
				FTransform ContainerWorldSpaceTransform = InContainerTransform.IsSet() ? ContainerInstance.Transform * InContainerTransform.GetValue() : ContainerInstance.Transform;
				for (FActorDescList::TConstIterator<> ActorDescIt(ContainerInstance.Container); ActorDescIt; ++ActorDescIt)
				{
					GatherLoadedActorsBounds(OutLoadedBounds, *ActorDescIt, ContainerInstance.Container, ContainerWorldSpaceTransform);
				}
			}
		}
	}
}

void FWorldPartitionHLODEditorData::UpdateLoadedActorsState()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FWorldPartitionHLODEditorData::UpdateLoadedActorsState);

	// Increment update counter - used to quickly find out if an HLOD actor needs be hidden without having to flag the whole hierarchy
	LastStateUpdate++;
	
	TArray<FBoundsWithVolume> LoadedBounds;

	// Gather LoaderAdapter
	for (UWorldPartitionEditorLoaderAdapter* EditorLoaderAdapter : WorldPartition->GetRegisteredEditorLoaderAdapters())
	{
		if (IWorldPartitionActorLoaderInterface::ILoaderAdapter* LoaderAdapter = EditorLoaderAdapter->GetLoaderAdapter())
		{
			if (LoaderAdapter->IsLoaded() && LoaderAdapter->GetBoundingBox().IsSet())
			{
				LoadedBounds.Emplace(LoaderAdapter->GetBoundingBox().GetValue());
			}
		}
	}

	// Gather ActorLoaderInterface
	FWorldPartitionHelpers::ForEachActorDesc(WorldPartition, [&LoadedBounds](const FWorldPartitionActorDesc* ActorDesc)
	{
		if (ActorDesc->GetActorNativeClass()->ImplementsInterface(UWorldPartitionActorLoaderInterface::StaticClass()))
		{
			if (AActor* Actor = ActorDesc->GetActor(false))
			{
				if (IWorldPartitionActorLoaderInterface::ILoaderAdapter* LoaderAdapter = Cast<IWorldPartitionActorLoaderInterface>(Actor)->GetLoaderAdapter())
				{
					if (LoaderAdapter->IsLoaded() && LoaderAdapter->GetBoundingBox().IsSet())
					{
						LoadedBounds.Emplace(ActorDesc->GetEditorBounds());
					}
				}
			}
		}

		return true;
	});
	
	// Gather Pinned Actors bounds
	if (WorldPartition->PinnedActors)
	{
		for (const FWorldPartitionHandle& PinnedActor : WorldPartition->PinnedActors->GetActors())
		{
			if (PinnedActor.IsValid())
			{
				GatherLoadedActorsBounds(LoadedBounds, *PinnedActor, WorldPartition->GetActorDescContainer());
			}
		}
	}

	// Sort Bounds by volume
	Algo::Sort(LoadedBounds, [](const FBoundsWithVolume& BoxA, const FBoundsWithVolume& BoxB) { return BoxA.Volume > BoxB.Volume; });

	const auto UpdateNodeState = [StateUpdate = LastStateUpdate, &LoadedBounds](FHLODSceneNode* HLODSceneNode)
	{
		auto UpdateNodeStateImpl = [StateUpdate, &LoadedBounds](FHLODSceneNode* HLODSceneNode, auto& UpdateNodeStateRef) -> void
		{
			FBox HLODSceneNodeBox = HLODSceneNode->Bounds.GetBox();

			const bool bHasAnyIntersectingLoadedRegion = Algo::AnyOf(LoadedBounds, [&HLODSceneNodeBox](const FBoundsWithVolume& Bounds) { return Bounds.Box.Intersect(HLODSceneNodeBox); });
			if (bHasAnyIntersectingLoadedRegion)
			{
				HLODSceneNode->HasIntersectingLoadedRegion = StateUpdate;

				for (FHLODSceneNode* Child : HLODSceneNode->ChildrenHLODs)
				{
					UpdateNodeStateRef(Child, UpdateNodeStateRef);
				}
			}
		};

		return UpdateNodeStateImpl(HLODSceneNode, UpdateNodeStateImpl);
	};

	// Update Nodes, starting from the top level HLODs down to their children
	for (FHLODSceneNode* HLODSceneNode : TopLevelHLODActorNodes)
	{
		UpdateNodeState(HLODSceneNode);
	}
}

void FWorldPartitionHLODEditorData::UpdateVisibility(const FVector& InCameraLocation, bool bInForceVisibilityUpdate)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FWorldPartitionHLODEditorData::UpdateVisibility);

	// Early out if no HLOD actor is loaded - none will be visible
	if (!HLODActorsLoader.IsValid())
	{
		return;
	}

	// For each top level HLOD actor (ex: HLOD2 in a 3 level of HLOD setup)
	// Determine visibility of each HLOD by a few factors:
	//  * If source (non-hlod) actors are loaded beneat it - HIDDEN
	//  * If near culled (using the min visible distance) - HIDDEN
	// Then if a given HLOD is HIDDEN, perform the same logic for its children.
	// If the HLOD is VISIBLE, then flag all it's children as HIDDEN
	for (const auto HLODActorNode : TopLevelHLODActorNodes)
	{
		// Recurse from the top level HLOD down to HLOD0
		HLODActorNode->UpdateVisibility(InCameraLocation, /*bInForceHidden*/false, bInForceVisibilityUpdate, LastStateUpdate);
	}
}

void FHLODSceneNode::UpdateVisibility(const FVector& InCameraLocation, bool bInForceHidden, bool bInForceVisibilityUpdate, int32 InLastStateUpdate)
{
	bool bNodeShouldBeVisible = !bInForceHidden;

	if (AWorldPartitionHLOD* HLODActor = Cast<AWorldPartitionHLOD>(HLODActorHandle->GetActor()))
	{
		// Do not show node if there are loaded actors under it
		if (bNodeShouldBeVisible)
		{
			const bool bHasLoadedActors = HasIntersectingLoadedRegion == InLastStateUpdate;
			bNodeShouldBeVisible = !bHasLoadedActors;
		}

		// Perform distance culling
		if (bNodeShouldBeVisible)
		{
			// Do not perform near culling if this HLOD has no child HLOD or none of them is loaded
			const bool bHasLoadedChildrenHLODs = Algo::AnyOf(ChildrenHLODs, [](FHLODSceneNode* ChildHLODNode) { return ChildHLODNode->HLODActorHandle.IsLoaded(); });
			if (bHasLoadedChildrenHLODs)
			{
				const float DistanceSquared = Bounds.ComputeSquaredDistanceFromBoxToPoint(InCameraLocation);
				const bool bNearCulled = DistanceSquared < FMath::Square(HLODActor->GetMinVisibleDistance());
				bNodeShouldBeVisible = !bNearCulled;
			}
		}

		if (bInForceVisibilityUpdate || bCachedIsVisible != bNodeShouldBeVisible)
		{
			bCachedIsVisible = bNodeShouldBeVisible;
			HLODActor->SetVisibility(bNodeShouldBeVisible);
		}
	}

	const bool bForceHideChildren = bNodeShouldBeVisible || bInForceHidden;
	for (FHLODSceneNode* Child : ChildrenHLODs)
	{
		Child->UpdateVisibility(InCameraLocation, bForceHideChildren, bInForceVisibilityUpdate, InLastStateUpdate);
	}
}

void FWorldPartitionHLODEditorData::SetHLODLoadingState(bool bInShouldBeLoaded)
{
	if (bInShouldBeLoaded && !HLODActorsLoader.IsValid())
	{
		HLODActorsLoader.Reset(new FLoaderAdapterHLOD(WorldPartition->GetTypedOuter<UWorld>()));
	}
	else if (!bInShouldBeLoaded && HLODActorsLoader.IsValid())
	{
		HLODActorsLoader.Reset();
	}
}
