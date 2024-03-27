// Copyright Epic Games, Inc. All Rights Reserved.

#include "SkeletalRenderNanite.h"
#include "Animation/MeshDeformerInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "RenderUtils.h"
#include "SkeletalRender.h"
#include "GPUSkinCache.h"
#include "RayTracingSkinnedGeometry.h"
#include "Rendering/RenderCommandPipes.h"
#include "ShaderParameterUtils.h"
#include "SceneInterface.h"
#include "SkeletalMeshSceneProxy.h"
#include "RenderGraphUtils.h"
#include "RenderCore.h"

FDynamicSkelMeshObjectDataNanite::FDynamicSkelMeshObjectDataNanite(
	USkinnedMeshComponent* InComponent,
	FSkeletalMeshRenderData* InRenderData,
	int32 InLODIndex
)
:	LODIndex(InLODIndex)
{
	UpdateRefToLocalMatrices(ReferenceToLocal, InComponent, InRenderData, LODIndex);

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	ComponentSpaceTransforms = InComponent->GetComponentSpaceTransforms();
#endif
}

FDynamicSkelMeshObjectDataNanite::~FDynamicSkelMeshObjectDataNanite() = default;

void FDynamicSkelMeshObjectDataNanite::GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize)
{
	CumulativeResourceSize.AddDedicatedSystemMemoryBytes(sizeof(*this));
	CumulativeResourceSize.AddDedicatedSystemMemoryBytes(ReferenceToLocal.GetAllocatedSize());
}

FSkeletalMeshObjectNanite::FSkeletalMeshObjectNanite(USkinnedMeshComponent* InComponent, FSkeletalMeshRenderData* InRenderData, ERHIFeatureLevel::Type InFeatureLevel)
: FSkeletalMeshObject(InComponent, InRenderData, InFeatureLevel)
, DynamicData(nullptr)
, CachedLOD(INDEX_NONE)
{
	for (int32 LODIndex = 0; LODIndex < InRenderData->LODRenderData.Num(); ++LODIndex)
	{
		new(LODs) FSkeletalMeshObjectLOD(InFeatureLevel, InRenderData, LODIndex);
	}

	InitResources(InComponent);
}

FSkeletalMeshObjectNanite::~FSkeletalMeshObjectNanite()
{
	delete DynamicData;
}

void FSkeletalMeshObjectNanite::InitResources(USkinnedMeshComponent* InComponent)
{
	for (int32 LODIndex = 0; LODIndex < LODs.Num(); ++LODIndex)
	{
		FSkeletalMeshObjectLOD& LOD = LODs[LODIndex];

		// Skip LODs that have their render data stripped
		if (LOD.RenderData->LODRenderData[LODIndex].GetNumVertices() > 0)
		{
			FSkelMeshComponentLODInfo* InitLODInfo = nullptr;
			if (InComponent->LODInfo.IsValidIndex(LODIndex))
			{
				InitLODInfo = &InComponent->LODInfo[LODIndex];
			}

			LOD.InitResources(InitLODInfo);
		}
	}
}

void FSkeletalMeshObjectNanite::ReleaseResources()
{
	for (int32 LODIndex = 0; LODIndex < LODs.Num(); ++LODIndex)
	{
		FSkeletalMeshObjectLOD& LOD = LODs[LODIndex];
		LOD.ReleaseResources();
	}
}

void FSkeletalMeshObjectNanite::Update(
	int32 LODIndex,
	USkinnedMeshComponent* InComponent,
	const FMorphTargetWeightMap& InActiveMorphTargets,
	const TArray<float>& MorphTargetWeights,
	EPreviousBoneTransformUpdateMode PreviousBoneTransformUpdateMode,
	const FExternalMorphWeightData& InExternalMorphWeightData)
{
	if (InComponent)
	{
		// Create the new dynamic data for use by the rendering thread
		// this data is only deleted when another update is sent
		FDynamicSkelMeshObjectDataNanite* NewDynamicData = new FDynamicSkelMeshObjectDataNanite(InComponent, SkeletalMeshRenderData, LODIndex);

		if (LODIndex != CachedLOD)
		{
			// ... TODO: Nanite-Skinning: perform remapping from LOD1...N -> LOD0 skeleton

			CachedLOD = LODIndex;
		}

		uint64 FrameNumberToPrepare = GFrameCounter;
		uint32 RevisionNumber = 0;

		if (InComponent->SceneProxy)
		{
			RevisionNumber = InComponent->GetBoneTransformRevisionNumber();
		}

		// Queue a call to update this data
		{
			FSkeletalMeshObjectNanite* MeshObject = this;
			ENQUEUE_RENDER_COMMAND(SkelMeshObjectUpdateDataCommand)(UE::RenderCommandPipe::SkeletalMesh,
				[MeshObject, FrameNumberToPrepare, RevisionNumber, NewDynamicData](FRHICommandList& RHICmdList)
				{
					FScopeCycleCounter Context(MeshObject->GetStatId());
					MeshObject->UpdateDynamicData_RenderThread(RHICmdList, NewDynamicData, FrameNumberToPrepare, RevisionNumber);
				}
			);
		}
	}
}

void FSkeletalMeshObjectNanite::UpdateDynamicData_RenderThread(FRHICommandList& RHICmdList, FDynamicSkelMeshObjectDataNanite* InDynamicData, uint64 FrameNumberToPrepare, uint32 RevisionNumber)
{
	// We should be done with the old data at this point
	delete DynamicData;

	// Update with new data
	DynamicData = InDynamicData;
	check(DynamicData);

	check(IsInParallelRenderingThread());

	// Source skeletal mesh and static lod model
	FSkeletalMeshLODRenderData& SourceLOD = SkeletalMeshRenderData->LODRenderData[DynamicData->LODIndex];
	(void)SourceLOD;

	// Bone matrices
	FMatrix44f* ReferenceToLocal = DynamicData->ReferenceToLocal.GetData();
	(void)ReferenceToLocal; // TODO: Nanite-Skinning
}

void FSkeletalMeshObjectNanite::EnableOverlayRendering(
	bool bEnabled,
	const TArray<int32>* InBonesOfInterest,
	const TArray<UMorphTarget*>* InMorphTargetOfInterest)
{
#if 0
	bRenderOverlayMaterial = bEnabled;

	BonesOfInterest.Reset();
	MorphTargetOfInterest.Reset();

	if (InBonesOfInterest)
	{
		BonesOfInterest.Append(*InBonesOfInterest);
	}
	else if (InMorphTargetOfInterest)
	{
		MorphTargetOfInterest.Append(*InMorphTargetOfInterest);
	}
#endif
}

const FVertexFactory* FSkeletalMeshObjectNanite::GetSkinVertexFactory(const FSceneView* View, int32 LODIndex, int32 ChunkIdx, ESkinVertexFactoryMode VFMode) const
{
	check(LODs.IsValidIndex(LODIndex));
	return nullptr;
	//return &LODs[LODIndex].VertexFactory;
}

TArray<FTransform>* FSkeletalMeshObjectNanite::GetComponentSpaceTransforms() const
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (DynamicData)
	{
		return &(DynamicData->ComponentSpaceTransforms);
	}
	else
#endif
	{
		return nullptr;
	}
}

const TArray<FMatrix44f>& FSkeletalMeshObjectNanite::GetReferenceToLocalMatrices() const
{
	return DynamicData->ReferenceToLocal;
}

int32 FSkeletalMeshObjectNanite::GetLOD() const
{
	if (DynamicData)
	{
		return DynamicData->LODIndex;
	}
	else
	{
		return 0;
	}
}

void FSkeletalMeshObjectNanite::DrawVertexElements(FPrimitiveDrawInterface* PDI, const FMatrix& ToWorldSpace, bool bDrawNormals, bool bDrawTangents, bool bDrawBinormals) const
{
#if 0
	uint32 NumIndices = CachedFinalVertices.Num();

	FMatrix LocalToWorldInverseTranspose = ToWorldSpace.InverseFast().GetTransposed();

	for (uint32 i = 0; i < NumIndices; i++)
	{
		FFinalSkinVertex& Vert = CachedFinalVertices[i];

		const FVector WorldPos = ToWorldSpace.TransformPosition(FVector(Vert.Position));

		const FVector Normal = Vert.TangentZ.ToFVector();
		const FVector Tangent = Vert.TangentX.ToFVector();
		const FVector Binormal = FVector(Normal) ^ FVector(Tangent);

		const float Len = 1.0f;

		if (bDrawNormals)
		{
			PDI->DrawLine(WorldPos, WorldPos + LocalToWorldInverseTranspose.TransformVector((FVector)(Normal)).GetSafeNormal() * Len, FLinearColor(0.0f, 1.0f, 0.0f), SDPG_World);
		}

		if (bDrawTangents)
		{
			PDI->DrawLine(WorldPos, WorldPos + LocalToWorldInverseTranspose.TransformVector(Tangent).GetSafeNormal() * Len, FLinearColor(1.0f, 0.0f, 0.0f), SDPG_World);
		}

		if (bDrawBinormals)
		{
			PDI->DrawLine(WorldPos, WorldPos + LocalToWorldInverseTranspose.TransformVector(Binormal).GetSafeNormal() * Len, FLinearColor(0.0f, 0.0f, 1.0f), SDPG_World);
		}
	}
#endif
}

bool FSkeletalMeshObjectNanite::HaveValidDynamicData() const
{
	return (DynamicData != nullptr);
}

void FSkeletalMeshObjectNanite::GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize)
{
	CumulativeResourceSize.AddDedicatedSystemMemoryBytes(sizeof(*this));

	if (DynamicData)
	{
		DynamicData->GetResourceSizeEx(CumulativeResourceSize);
	}

	CumulativeResourceSize.AddDedicatedSystemMemoryBytes(LODs.GetAllocatedSize());

	for (int32 Index = 0; Index < LODs.Num(); ++Index)
	{
		LODs[Index].GetResourceSizeEx(CumulativeResourceSize);
	}
}

void FSkeletalMeshObjectNanite::UpdateSkinWeightBuffer(USkinnedMeshComponent* InComponent)
{
	for (int32 LODIndex = 0; LODIndex < LODs.Num(); ++LODIndex)
	{
		FSkeletalMeshObjectLOD& LOD = LODs[LODIndex];

		// Skip LODs that have their render data stripped
		if (LOD.RenderData->LODRenderData[LODIndex].GetNumVertices() > 0)
		{
			FSkelMeshComponentLODInfo* UpdateLODInfo = nullptr;
			if (InComponent->LODInfo.IsValidIndex(LODIndex))
			{
				UpdateLODInfo = &InComponent->LODInfo[LODIndex];
			}

			LOD.UpdateSkinWeights(UpdateLODInfo);
		}
	}
}

void FSkeletalMeshObjectNanite::FSkeletalMeshObjectLOD::InitResources(FSkelMeshComponentLODInfo* InLODInfo)
{
	check(RenderData);
	check(RenderData->LODRenderData.IsValidIndex(LODIndex));

	FSkeletalMeshLODRenderData& LODData = RenderData->LODRenderData[LODIndex];
	(void)LODData;

#if RHI_RAYTRACING
	if (IsRayTracingEnabled() && RenderData->bSupportRayTracing)
	{
	}
#endif

	bInitialized = true;
}

void FSkeletalMeshObjectNanite::FSkeletalMeshObjectLOD::ReleaseResources()
{
	bInitialized = false;
}

void FSkeletalMeshObjectNanite::FSkeletalMeshObjectLOD::GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize)
{
}

void FSkeletalMeshObjectNanite::FSkeletalMeshObjectLOD::UpdateSkinWeights(FSkelMeshComponentLODInfo* InLODInfo)
{
	check(RenderData);
	check(RenderData->LODRenderData.IsValidIndex(LODIndex));

	//FSkeletalMeshLODRenderData& LODData = RenderData->LODRenderData[LODIndex];
	//MeshObjectWeightBuffer = FSkeletalMeshObject::GetSkinWeightVertexBuffer(LODData, InLODInfo);
}