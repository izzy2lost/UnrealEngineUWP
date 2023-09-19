// Copyright Epic Games, Inc. All Rights Reserved.

#include "GrassInstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"

UGrassInstancedStaticMeshComponent::UGrassInstancedStaticMeshComponent(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer)
{
	ViewRelevanceType = EHISMViewRelevanceType::Grass;
}

void UGrassInstancedStaticMeshComponent::BuildTreeAnyThread(
	TArray<FMatrix>& InstanceTransforms, 
	TArray<float>& InstanceCustomDataFloats,
	int32 NumCustomDataFloats,
	const FBox& MeshBox,
	TArray<FClusterNode>& OutClusterTree,
	TArray<int32>& OutSortedInstances,
	TArray<int32>& OutInstanceReorderTable,
	int32& OutOcclusionLayerNum,
	int32 MaxInstancesPerLeaf,
	bool InGenerateInstanceScalingRange
	)
{
	check(MaxInstancesPerLeaf > 0);

	// do grass need this?
	float DensityScaling = 1.0f;
	int32 InstancingRandomSeed = 1;

	FClusterBuilder Builder(InstanceTransforms, InstanceCustomDataFloats, NumCustomDataFloats, MeshBox, MaxInstancesPerLeaf, DensityScaling, InstancingRandomSeed, InGenerateInstanceScalingRange);
	Builder.BuildTree();
	OutOcclusionLayerNum = Builder.Result->OutOcclusionLayerNum;

	OutClusterTree = MoveTemp(Builder.Result->Nodes);
	OutInstanceReorderTable = MoveTemp(Builder.Result->InstanceReorderTable);
	OutSortedInstances = MoveTemp(Builder.Result->SortedInstances);
}

void UGrassInstancedStaticMeshComponent::AcceptPrebuiltTree(TArray<FInstancedStaticMeshInstanceData>& InInstanceData, TArray<FClusterNode>& InClusterTree, int32 InOcclusionLayerNumNodes, int32 InNumBuiltRenderInstances)
{
	checkSlow(IsInGameThread());

	QUICK_SCOPE_CYCLE_COUNTER(STAT_UGrassInstancedStaticMeshComponent_AcceptPrebuiltTree);

	// this is only for prebuild data, already in the correct order
	check(!PerInstanceSMData.Num());

	NumBuiltInstances = 0;
	TranslatedInstanceSpaceOrigin = FVector::Zero();
	check(PerInstanceRenderData.IsValid());
	NumBuiltRenderInstances = InNumBuiltRenderInstances;
	check(NumBuiltRenderInstances);
	UnbuiltInstanceBounds.Init();
	UnbuiltInstanceBoundsList.Empty();
	ClusterTreePtr = MakeShareable(new TArray<FClusterNode>);
	InstanceReorderTable.Empty();
	SortedInstances.Empty();
	OcclusionLayerNumNodes = InOcclusionLayerNumNodes;
	BuiltInstanceBounds = GetClusterTreeBounds(InClusterTree, FVector::Zero());
	InstanceCountToRender = InNumBuiltRenderInstances;

	// Verify that the mesh is valid before using it.
	const bool bMeshIsValid =
		// make sure we have instances
		NumBuiltRenderInstances > 0 &&
		// make sure we have an actual staticmesh
		GetStaticMesh() &&
		GetStaticMesh()->HasValidRenderData();

	if (bMeshIsValid)
	{
		*ClusterTreePtr = MoveTemp(InClusterTree);

		if (RequiresInstanceDataForTree())
		{
			PerInstanceSMData = MoveTemp(InInstanceData);
		}

		PostBuildStats();

	}
	QUICK_SCOPE_CYCLE_COUNTER(STAT_UGrassInstancedStaticMeshComponent_AcceptPrebuiltTree_Mark);

	MarkRenderStateDirty();
}

