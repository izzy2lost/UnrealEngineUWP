// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "WorldPartition/LoaderAdapter/LoaderAdapterActorList.h"


class UWorldPartition;

// Represent an HLOD actor in the editor, loaded or not
struct FHLODSceneNode
{
	void UpdateVisibility(const FVector& InCameraLocation, bool bInForceHidden, bool bInForceVisibilityUpdate, int32 InLastStateUpdate);

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
	~FWorldPartitionHLODEditorData();

	void UpdateLoadedActorsState();
	void UpdateVisibility(const FVector& InCameraLocation, bool bForceVisibilityUpdate);

	void LoadHLODActors();
	void UnloadHLODActors();

private:
	UWorldPartition* WorldPartition;
	TMap<FGuid, TUniquePtr<FHLODSceneNode>> HLODActorNodes;
	TArray<FHLODSceneNode*> TopLevelHLODActorNodes;
	TUniquePtr<FLoaderAdapterActorList> HLODActorsLoader;
	int32 LastStateUpdate;
};
