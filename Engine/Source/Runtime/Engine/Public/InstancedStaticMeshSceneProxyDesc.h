// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "StaticMeshSceneProxyDesc.h"
#include "Engine/InstancedStaticMesh.h"

class UInstancedStaticMeshComponent;

struct FInstancedStaticMeshSceneProxyDesc : public FStaticMeshSceneProxyDesc
{		
	FInstancedStaticMeshSceneProxyDesc() = default;
	ENGINE_API FInstancedStaticMeshSceneProxyDesc(const UInstancedStaticMeshComponent*);

	TArrayView<const FInstancedStaticMeshInstanceData> PerInstanceSMData;	
	TSharedPtr<FPerInstanceRenderData, ESPMode::ThreadSafe> PerInstanceRenderData;
	TArrayView<const float> PerInstanceSMCustomData;
#if WITH_EDITOR
	TBitArray<>	SelectedInstances;
#endif
	TArrayView<const int32> InstanceReorderTable; 
	TArrayView<const FMatrix> PerInstancePrevTransform;
	FInstanceUpdateCmdBuffer const* InstanceUpdateCmdBuffer;

	int32 InstanceStartCullDistance = 0;
	int32 InstanceEndCullDistance = 0;
	FVector MinScale = FVector(0);
	FVector MaxScale = FVector(0);
	int32 NumCustomDataFloats = 0;
	FVector TranslatedInstanceSpaceOrigin = FVector(0);
	float InstanceLODDistanceScale = 1.0f;

	bool bUseGpuLodSelection = false;
	bool bHasPerInstanceHitProxies = false;

	const FInstanceUpdateCmdBuffer& GetInstanceUpdateCmdBuffer() const { check(InstanceUpdateCmdBuffer); return *InstanceUpdateCmdBuffer; } 
	
	void GetInstancesMinMaxScale(FVector& InMinScale, FVector& InMaxScale) const
	{
		InMinScale = MinScale;
		InMaxScale = MaxScale;
	}

	FVector GetTranslatedInstanceSpaceOrigin() const
	{
		return TranslatedInstanceSpaceOrigin;	
	}

	int32 GetInstanceCount() const
	{
		return PerInstanceSMData.Num();
	}

	int32 GetRenderIndex(int32 InInstanceIndex) const 
	{ 
		return InstanceReorderTable.IsValidIndex(InInstanceIndex) ? InstanceReorderTable[InInstanceIndex] : InInstanceIndex; 
	}

	void GetInstanceTransform(int32 InInstanceIndex, FTransform& OutInstanceTransform) const
	{	
		if (!PerInstanceSMData.IsValidIndex(InInstanceIndex))
		{
			return;
		}

		const FInstancedStaticMeshInstanceData& InstanceData = PerInstanceSMData[InInstanceIndex];

		OutInstanceTransform = FTransform(InstanceData.Transform);		
	}

	bool GetInstancePrevTransform(int32 InInstanceIndex, FTransform& OutInstanceTransform) const
	{
		if (!PerInstancePrevTransform.IsValidIndex(InInstanceIndex))
		{
			return false;
		}

		const FMatrix& InstanceData = PerInstancePrevTransform[InInstanceIndex];

		OutInstanceTransform = FTransform(InstanceData);
		return true;
	}
};