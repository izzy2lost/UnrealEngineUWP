// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "WorldPartition/LoaderAdapter/LoaderAdapterActorList.h"


class UWorldPartition;

// Represent an HLOD actor in the editor, loaded or not
struct FHLODSceneNode
{
	void UpdateVisibility(const FVector& InCameraLocation, double InMinDrawDistance, double InMaxDrawDistance, bool bInForceHidden, bool bInForceVisibilityUpdate, int32 InLastStateUpdate);

	FHLODSceneNode* ParentHLOD = nullptr;
	TArray<FHLODSceneNode*> ChildrenHLODs;


	FBoxSphereBounds Bounds;
	bool bCachedIsVisible = true;
	FWorldPartitionHandle HLODActorHandle;
	
	int32 HasIntersectingLoadedRegion = INDEX_NONE;
};

// Editor state of HLODs for a given World Partition
struct FWorldPartitionHLODEditorData
{
public:
	FWorldPartitionHLODEditorData(UWorldPartition* InWorldPartition);
	
	void ClearLoadedActorsState();
	void UpdateLoadedActorsState();
	void UpdateVisibility(const FVector& InCameraLocation, double InMinDrawDistance, double InMaxDrawDistance, bool bForceVisibilityUpdate);

	void SetHLODLoadingState(bool bInShouldBeLoaded);

private:
	UWorldPartition* WorldPartition;
	TMap<FGuid, TUniquePtr<FHLODSceneNode>> HLODActorNodes;
	TArray<FHLODSceneNode*> TopLevelHLODActorNodes;
	TUniquePtr<FLoaderAdapterActorList> HLODActorsLoader;
	int32 LastStateUpdate;
};
