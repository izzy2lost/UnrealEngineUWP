// Copyright Epic Games, Inc. All Rights Reserved.

#include "GroomManager.h"
#include "HairStrandsMeshProjection.h"

#include "GeometryCacheComponent.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Rendering/SkinWeightVertexBuffer.h"
#include "CommonRenderResources.h"
#include "Components/SkeletalMeshComponent.h"
#include "SkeletalMeshSceneProxy.h"
#include "SkeletalRenderPublic.h"
#include "GroomTextureBuilder.h"
#include "GroomResources.h"
#include "GroomInstance.h"
#include "GroomGeometryCache.h"
#include "GeometryCacheSceneProxy.h"
#include "HAL/IConsoleManager.h"
#include "SceneView.h"
#include "HairStrandsInterpolation.h"
#include "HairCardsVertexFactory.h"
#include "HairStrandsVertexFactory.h"
#include "RenderGraphEvent.h"
#include "RenderGraphUtils.h"
#include "CachedGeometry.h"
#include "GroomCacheData.h"
#include "SceneInterface.h"
#include "PrimitiveSceneInfo.h"
#include "HairStrandsClusterCulling.h"
#include "GroomVisualizationData.h"
#include "SkeletalMeshDeformerHelpers.h"
#include "HairStrandsInterface.h"

static int32 GHairStrandsMinLOD = 0;
static FAutoConsoleVariableRef CVarGHairStrandsMinLOD(TEXT("r.HairStrands.MinLOD"), GHairStrandsMinLOD, TEXT("Clamp the min hair LOD to this value, preventing to reach lower/high-quality LOD."), ECVF_Scalability);

static int32 GHairStrands_UseCards = 0;
static FAutoConsoleVariableRef CVarHairStrands_UseCards(TEXT("r.HairStrands.UseCardsInsteadOfStrands"), GHairStrands_UseCards, TEXT("Force cards geometry on all groom elements. If no cards data is available, nothing will be displayed"), ECVF_Scalability);

static int32 GHairStrands_SwapBufferType = 3;
static FAutoConsoleVariableRef CVarGHairStrands_SwapBufferType(TEXT("r.HairStrands.SwapType"), GHairStrands_SwapBufferType, TEXT("Swap rendering buffer at the end of frame. This is an experimental toggle. Default:1"));

static int32 GHairStrands_ManualSkinCache = 0;
static FAutoConsoleVariableRef CVarGHairStrands_ManualSkinCache(TEXT("r.HairStrands.ManualSkinCache"), GHairStrands_ManualSkinCache, TEXT("If skin cache is not enabled, and grooms use skinning method, this enable a simple skin cache mechanisme for groom. Default:disable"));

static int32 GHairStrands_InterpolationFrustumCullingEnable = 1;
static FAutoConsoleVariableRef CVarHairStrands_InterpolationFrustumCullingEnable(TEXT("r.HairStrands.Interoplation.FrustumCulling"), GHairStrands_InterpolationFrustumCullingEnable, TEXT("Swap rendering buffer at the end of frame. This is an experimental toggle. Default:1"));

static int32 GHairStrands_Streaming = 0;
static FAutoConsoleVariableRef CVarHairStrands_Streaming(TEXT("r.HairStrands.Streaming"), GHairStrands_Streaming, TEXT("Hair strands streaming toggle."), ECVF_RenderThreadSafe | ECVF_ReadOnly);
static int32 GHairStrands_Streaming_CurvePage = 2048;
static FAutoConsoleVariableRef CVarHairStrands_StreamingCurvePage(TEXT("r.HairStrands.Streaming.CurvePage"), GHairStrands_Streaming_CurvePage, TEXT("Number of strands curve per streaming page"));
static int32 GHairStrands_Streaming_StreamOutThreshold = 2;
static FAutoConsoleVariableRef CVarHairStrands_Streaming_StreamOutThreshold(TEXT("r.HairStrands.Streaming.StreamOutThreshold"), GHairStrands_Streaming_StreamOutThreshold, TEXT("Threshold used for streaming out data. In curve page. Default:2."));

static int32 GHairStrands_AutoLOD_Force = 0;
static float GHairStrands_AutoLOD_Scale = 1.f;
static float GHairStrands_AutoLOD_Bias = 0.f;
static FAutoConsoleVariableRef CVarHairStrands_AutoLOD_Force(TEXT("r.HairStrands.AutoLOD.Force"), GHairStrands_AutoLOD_Force, TEXT("Force all groom to use Auto LOD (experimental)."), ECVF_RenderThreadSafe);
static FAutoConsoleVariableRef CVarHairStrands_AutoLOD_Scale(TEXT("r.HairStrands.AutoLOD.Scale"), GHairStrands_AutoLOD_Scale, TEXT("Hair strands Auto LOD rate at which curves get decimated based on screen coverage."), ECVF_RenderThreadSafe);
static FAutoConsoleVariableRef CVarHairStrands_AutoLOD_Bias(TEXT("r.HairStrands.AutoLOD.Bias"), GHairStrands_AutoLOD_Bias, TEXT("Hair strands Auto LOD screen size bias at which curves get decimated."), ECVF_RenderThreadSafe);

///////////////////////////////////////////////////////////////////////////////////////////////////
// Foward declaration

bool IsHairStrandsForceRebuildBVH();
bool IsHairStrandsTransferPositionOnLODChange();
uint32 GetHairRaytracingProceduralSplits();
void CreateHairStrandsDebugAttributeBuffer(FRDGBuilder& GraphBuilder, FRDGExternalBuffer* DebugAttributeBuffer, uint32 VertexCount, const FName& OwnerName);
FHairGroupPublicData::FVertexFactoryInput InternalComputeHairStrandsVertexInputData(FRDGBuilder* GraphBuilder, const FHairGroupInstance* Instance, EGroomViewMode ViewMode);
EGroomCacheType GetHairInstanceCacheType(const FHairGroupInstance* Instance);

void AddHairClusterAABBPass(
	FRDGBuilder& GraphBuilder,
	FGlobalShaderMap* ShaderMap,
	const uint32 ActiveCurveCount,
	const FVector& TranslatedWorldOffset,
	const FShaderPrintData* ShaderPrintData,
	const EHairAABBUpdateType UpdateType,
	FHairGroupInstance* Instance,
	FRDGBufferSRVRef RenderDeformedOffsetBuffer,
	FHairStrandClusterData::FHairGroup* ClusterData,
	const uint32 ClusterOffset,
	const uint32 CluesterCount,
	FRDGBufferSRVRef RenderPositionBufferSRV,
	FRDGBufferSRVRef& DrawIndirectRasterComputeBuffer,
	FRDGBufferUAVRef ClusterAABBUAV,
	FRDGBufferUAVRef GroupAABBBUAV);

///////////////////////////////////////////////////////////////////////////////////////////////////
// Utils

EHairBufferSwapType GetHairSwapBufferType()
{
	switch (GHairStrands_SwapBufferType)
	{
	case 0: return EHairBufferSwapType::BeginOfFrame;
	case 1: return EHairBufferSwapType::EndOfFrame;
	case 2: return EHairBufferSwapType::Tick;
	case 3: return EHairBufferSwapType::RenderFrame;
	}
	return EHairBufferSwapType::EndOfFrame;
}

bool IsHairStrandsForceAutoLODEnabled()
{
	return GHairStrands_AutoLOD_Force > 0;
}

uint32 GetStreamingCurvePage()
{
	return FMath::Clamp(uint32(GHairStrands_Streaming_CurvePage), 1u, 32768u);
}

static uint32 GetRoundedCurveCount(uint32 InRequest, uint32 InMaxCurve)
{
	const uint32 Page = GetStreamingCurvePage();
	return FMath::Min(FMath::DivideAndRoundUp(InRequest, Page) * Page,  InMaxCurve);
}

bool NeedDeallocation(uint32 InRequest, uint32 InAvailable)
{
	bool bNeedDeallocate = false;
	if (GHairStrands_Streaming > 0)
	{

		const uint32 Page = GetStreamingCurvePage();
		const uint32 RequestedPageCount = FMath::DivideAndRoundUp(InRequest, Page);
		const uint32 AvailablePageCount = FMath::DivideAndRoundUp(InAvailable, Page);
	
		const uint32 DeallactionThreshold = FMath::Max(GHairStrands_Streaming_StreamOutThreshold, 2);
		bNeedDeallocate = AvailablePageCount - RequestedPageCount >= DeallactionThreshold;
	}
	return bNeedDeallocate;
}

DEFINE_LOG_CATEGORY_STATIC(LogGroomManager, Log, All);

///////////////////////////////////////////////////////////////////////////////////////////////////

void FHairGroupInstance::FCards::FLOD::InitVertexFactory()
{
	VertexFactory->InitResources(FRHICommandListImmediate::Get());
}

void FHairGroupInstance::FMeshes::FLOD::InitVertexFactory()
{
	VertexFactory->InitResources(FRHICommandListImmediate::Get());
}

static bool IsInstanceFrustumCullingEnable()
{
	return GHairStrands_InterpolationFrustumCullingEnable > 0;
}

bool NeedsUpdateCardsMeshTriangles();

static bool IsSkeletalMeshEvaluationEnabled()
{
	// When deferred skel. mesh update is enabled, hair strands skeletal mesh deformation is not allowed, as skin-cached update happen after 
	// the PreInitView calls, which causes hair LOD selection and hair simulation to have invalid value & resources
	static const auto CVarSkelMeshGDME = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.DeferSkeletalDynamicDataUpdateUntilGDME"));
	return CVarSkelMeshGDME ? CVarSkelMeshGDME->GetValueOnAnyThread() == 0 : true;
}

// Retrive the skel. mesh scene info
static const FPrimitiveSceneInfo* GetMeshSceneInfo(FSceneInterface* Scene, FHairGroupInstance* Instance)
{
	if (!Instance->Debug.MeshComponentId.IsValid())
	{
		return nullptr;
	}

	// Try to use the cached persistent primitive index (fast)
	const FPrimitiveSceneInfo* PrimitiveSceneInfo = nullptr;
	if (Instance->Debug.CachedMeshPersistentPrimitiveIndex.IsValid())
	{
		PrimitiveSceneInfo = Scene->GetPrimitiveSceneInfo(Instance->Debug.CachedMeshPersistentPrimitiveIndex);
		PrimitiveSceneInfo = (PrimitiveSceneInfo && PrimitiveSceneInfo->PrimitiveComponentId == Instance->Debug.MeshComponentId) ? PrimitiveSceneInfo : nullptr;
	}

	// If this fails, find the primitive index, and then the primitive scene info (slow)
	if (PrimitiveSceneInfo == nullptr)
	{
		const int32 PrimitiveIndex = Scene->GetScenePrimitiveComponentIds().Find(Instance->Debug.MeshComponentId);
		PrimitiveSceneInfo = Scene->GetPrimitiveSceneInfo(PrimitiveIndex);
		Instance->Debug.CachedMeshPersistentPrimitiveIndex = PrimitiveSceneInfo ? PrimitiveSceneInfo->GetPersistentIndex() : FPersistentPrimitiveIndex();
	}

	return PrimitiveSceneInfo;
}

// Returns the cached geometry of the underlying geometry on which a hair instance is attached to
FCachedGeometry GetCacheGeometryForHair(
	FRDGBuilder& GraphBuilder, 
	FSceneInterface* Scene,
	FHairGroupInstance* Instance, 
	FGlobalShaderMap* ShaderMap,
	const bool bOutputTriangleData)
{
	FCachedGeometry Out;
	if (Instance->Debug.GroomBindingType == EGroomBindingMeshType::SkeletalMesh)
	{
		if (IsSkeletalMeshEvaluationEnabled())
		{
			if (const FPrimitiveSceneInfo* PrimitiveSceneInfo = GetMeshSceneInfo(Scene, Instance))
			{
				if (const FSkeletalMeshSceneProxy* SceneProxy = static_cast<const FSkeletalMeshSceneProxy*>(PrimitiveSceneInfo->Proxy))
				{				
					SceneProxy->GetCachedGeometry(Out);

					if (GHairStrands_ManualSkinCache > 0 && Out.Sections.Num() == 0)
					{
						//#hair_todo: Need to have a (frame) cache to insure that we don't recompute the same projection several time
						// Actual populate the cache with only the needed part based on the groom projection data. At the moment it recompute everything ...
						BuildCacheGeometry(GraphBuilder, ShaderMap, SceneProxy, bOutputTriangleData, Out);
					}
				}
			}
		}
	}
	else if (Instance->Debug.GroomBindingType == EGroomBindingMeshType::GeometryCache)
	{
		if (const FPrimitiveSceneInfo* PrimitiveSceneInfo = GetMeshSceneInfo(Scene, Instance))
		{
			if (const FGeometryCacheSceneProxy* SceneProxy = static_cast<const FGeometryCacheSceneProxy*>(PrimitiveSceneInfo->Proxy))
			{
				BuildCacheGeometry(GraphBuilder, ShaderMap, SceneProxy, bOutputTriangleData, Out);
			}
		}
	}
	return Out;
}

// Return the LOD index of the cached geometry on which the hair are bound to. 
// Return -1 if the hair are not bound of if the underlying geometry is invalid
static int32 GetCacheGeometryLODIndex(const FCachedGeometry& In)
{
	int32 MeshLODIndex = In.LODIndex;
	for (const FCachedGeometry::Section& Section : In.Sections)
	{
		// Ensure all mesh's sections have the same LOD index
		if (MeshLODIndex < 0) MeshLODIndex = Section.LODIndex;
		check(MeshLODIndex == Section.LODIndex);
	}

	return MeshLODIndex;
}

enum class EHairInterpolationPassType
{
	Strands,
	CardsAndMeshes,
	Guides
};

static void RunInternalHairInterpolation(
	FRDGBuilder& GraphBuilder,
	FSceneInterface* Scene,
	const FSceneView* View,
	const uint32 ViewUniqueID,
	const FHairStrandsInstances& Instances,
	const FShaderPrintData* ShaderPrintData,
	FGlobalShaderMap* ShaderMap, 
	EHairInterpolationPassType PassType,
	FHairStrandClusterData* ClusterData)
{
	check(IsInRenderingThread());

	#if RHI_RAYTRACING
	const uint32 ViewRayTracingMask = View->Family->EngineShowFlags.PathTracing ? EHairViewRayTracingMask::PathTracing : EHairViewRayTracingMask::RayTracing;
	#else
	const uint32 ViewRayTracingMask = 0u;
	#endif

	const EGroomViewMode ViewMode = GetGroomViewMode(*View);

	// Update dynamic mesh triangles
	for (FHairStrandsInstance* AbstractInstance : Instances)
	{
		FHairGroupInstance* Instance = static_cast<FHairGroupInstance*>(AbstractInstance);

		int32 MeshLODIndex = -1;
		if (Instance->GeometryType == EHairGeometryType::NoneGeometry)
			continue;
	
		check(Instance->HairGroupPublicData);

		FHairStrandsProjectionMeshData::LOD MeshDataLOD;
		const FCachedGeometry CachedGeometry = GetCacheGeometryForHair(GraphBuilder, Scene, Instance, ShaderMap, true);
		for (int32 SectionIndex = 0; SectionIndex < CachedGeometry.Sections.Num(); ++SectionIndex)
		{
			// Ensure all mesh's sections have the same LOD index
			const int32 SectionLodIndex = CachedGeometry.Sections[SectionIndex].LODIndex;
			if (MeshLODIndex < 0) MeshLODIndex = SectionLodIndex;
			check(MeshLODIndex == SectionLodIndex);

			MeshDataLOD.Sections.Add(ConvertMeshSection(CachedGeometry, SectionIndex));
		}

		Instance->Debug.MeshLODIndex = MeshLODIndex;
		if (0 <= MeshLODIndex)
		{
			const EHairGeometryType InstanceGeometryType = Instance->GeometryType;
			const EHairBindingType InstanceBindingType = Instance->BindingType;

			const uint32 HairLODIndex = Instance->HairGroupPublicData->LODIndex;
			const EHairBindingType BindingType = Instance->HairGroupPublicData->GetBindingType(HairLODIndex);
			const bool bSimulationEnable = Instance->HairGroupPublicData->IsSimulationEnable(HairLODIndex);
			const bool bDeformationEnable = Instance->HairGroupPublicData->bIsDeformationEnable;
			const bool bGlobalDeformationEnable = Instance->HairGroupPublicData->IsGlobalInterpolationEnable(HairLODIndex);
			check(InstanceBindingType == BindingType);

			if (EHairInterpolationPassType::CardsAndMeshes == PassType)
			{
				if (InstanceGeometryType == EHairGeometryType::Cards)
				{
					if (Instance->Cards.IsValid(HairLODIndex))
					{
						FHairGroupInstance::FCards::FLOD& CardsInstance = Instance->Cards.LODs[HairLODIndex];
						if (BindingType == EHairBindingType::Skinning || bGlobalDeformationEnable)
						{
							check(CardsInstance.Guides.IsValid());
							check(CardsInstance.Guides.HasValidRootData());
							check(CardsInstance.Guides.DeformedRootResource->IsValid(MeshLODIndex));

							AddHairStrandUpdateMeshTrianglesPass(
								GraphBuilder,
								ShaderMap,
								MeshLODIndex,
								MeshDataLOD,
								CardsInstance.Guides.RestRootResource,
								CardsInstance.Guides.DeformedRootResource);

							AddHairStrandUpdatePositionOffsetPass(
								GraphBuilder,
								ShaderMap,
								EHairPositionUpdateType::Cards,
								Instance->RegisteredIndex,
								HairLODIndex,
								MeshLODIndex,
								CardsInstance.Guides.DeformedRootResource,
								CardsInstance.Guides.DeformedResource);
						}
						else if (bSimulationEnable || bDeformationEnable)
						{
							check(CardsInstance.Guides.IsValid());
							AddHairStrandUpdatePositionOffsetPass(
								GraphBuilder,
								ShaderMap,
								EHairPositionUpdateType::Cards,
								Instance->RegisteredIndex,
								HairLODIndex,
								MeshLODIndex,
								nullptr,
								CardsInstance.Guides.DeformedResource);
						}
					}
				}
				else if (InstanceGeometryType == EHairGeometryType::Meshes)
				{
					// Nothing to do
				}
			}
		}
	}

	// Hair interpolation
	if (EHairInterpolationPassType::CardsAndMeshes == PassType)
	{
		check(View);
		const FVector& TranslatedWorldOffset = View->ViewMatrices.GetPreViewTranslation();
		for (FHairStrandsInstance* AbstractInstance : Instances)
		{
			FHairGroupInstance* Instance = static_cast<FHairGroupInstance*>(AbstractInstance);

			const bool bGeometryCompatibleWithPassType =
				(EHairInterpolationPassType::CardsAndMeshes == PassType && (Instance->GeometryType == EHairGeometryType::Cards || Instance->GeometryType == EHairGeometryType::Meshes));
			if (bGeometryCompatibleWithPassType)
			{
				ComputeHairStrandsInterpolation(
					GraphBuilder, 
					ShaderMap,
					ViewUniqueID,
					ViewRayTracingMask,
					ViewMode,
					TranslatedWorldOffset,
					ShaderPrintData,
					Instance,
					Instance->Debug.MeshLODIndex,
					ClusterData);
			}
		}
	}
}

static void RunHairStrandsInterpolation_Guide(
	FRDGBuilder& GraphBuilder,
	FSceneInterface* Scene,
	const FSceneView* View,
	const uint32 ViewUniqueID,
	const FHairStrandsInstances& Instances,
	const FShaderPrintData* ShaderPrintData,
	FGlobalShaderMap* ShaderMap)
{
	check(IsInRenderingThread());

	DECLARE_GPU_STAT(HairGuideInterpolation);
	RDG_EVENT_SCOPE(GraphBuilder, "HairGuideInterpolation");
	RDG_GPU_STAT_SCOPE(GraphBuilder, HairGuideInterpolation);

	// Update dynamic mesh triangles
	struct FInstanceData
	{
		int32 HairLODIndex = -1;
		int32 MeshLODIndex = -1;
		EGroomCacheType ActiveGroomCacheType = EGroomCacheType::None;
		EHairBindingType BindingType = EHairBindingType::NoneBinding;
		bool bSimulationEnable = false;
		bool bDeformationEnable = false;
		bool bGlobalDeformationEnable = false;

		FHairStrandsProjectionMeshData::LOD MeshDataLOD;
		FHairGroupInstance* Instance = nullptr;

		bool NeedsMeshUpdate() const
		{
			return 
				MeshLODIndex >= 0 && 
				(BindingType == EHairBindingType::Skinning) && 
				(bGlobalDeformationEnable || bSimulationEnable || bDeformationEnable);
		}
	};

	// Gather all instances which require guides or RBF deformations
	TArray<FInstanceData> InstanceDatas;
	InstanceDatas.Reserve(Instances.Num());
	for (FHairStrandsInstance* AbstractInstance : Instances)
	{
		FHairGroupInstance* Instance = static_cast<FHairGroupInstance*>(AbstractInstance);
		if (Instance->GeometryType == EHairGeometryType::NoneGeometry)
		{
			continue;
		}	
		check(Instance->HairGroupPublicData);

		FInstanceData& InstanceData = InstanceDatas.AddDefaulted_GetRef();
		InstanceData.Instance 					= Instance;
		InstanceData.ActiveGroomCacheType 		= GetHairInstanceCacheType(Instance);
		InstanceData.BindingType 				= Instance->BindingType;
		InstanceData.HairLODIndex 				= Instance->HairGroupPublicData->LODIndex;
		InstanceData.bSimulationEnable 			= Instance->HairGroupPublicData->IsSimulationEnable(InstanceData.HairLODIndex);
		InstanceData.bDeformationEnable 		= Instance->HairGroupPublicData->bIsDeformationEnable;
		InstanceData.bGlobalDeformationEnable 	= Instance->HairGroupPublicData->IsGlobalInterpolationEnable(InstanceData.HairLODIndex);
		InstanceData.MeshLODIndex				= -1;

		// Extract MeshDataLOD and compute MeshLODIndex
		const FCachedGeometry CachedGeometry = GetCacheGeometryForHair(GraphBuilder, Scene, Instance, ShaderMap, true);
		for (int32 SectionIndex = 0; SectionIndex < CachedGeometry.Sections.Num(); ++SectionIndex)
		{
			// Ensure all mesh's sections have the same LOD index
			const int32 SectionLodIndex = CachedGeometry.Sections[SectionIndex].LODIndex;
			if (InstanceData.MeshLODIndex < 0) InstanceData.MeshLODIndex = SectionLodIndex;
			check(InstanceData.MeshLODIndex == SectionLodIndex);

			InstanceData.MeshDataLOD.Sections.Add(ConvertMeshSection(CachedGeometry, SectionIndex));
		}
		Instance->Debug.MeshLODIndex = InstanceData.MeshLODIndex;

		if (InstanceData.NeedsMeshUpdate())
		{
			check(InstanceData.Instance->Guides.IsValid());
		}
	}	

	// Update dynamic mesh triangles
	// Guide update need to run only if simulation is enabled, or if RBF is enabled (since RFB are transfer through guides)
	for (FInstanceData& InstanceData : InstanceDatas)
	{
		if (InstanceData.NeedsMeshUpdate())
		{		
			check(InstanceData.Instance->Guides.IsValid());
			check(InstanceData.Instance->Guides.HasValidRootData());
			check(InstanceData.Instance->Guides.DeformedRootResource->IsValid(InstanceData.MeshLODIndex));

			AddHairStrandUpdateMeshTrianglesPass(
				GraphBuilder,
				ShaderMap,
				InstanceData.Instance->Debug.MeshLODIndex,
				InstanceData.MeshDataLOD,
				InstanceData.Instance->Guides.RestRootResource,
				InstanceData.Instance->Guides.DeformedRootResource);
		}
	}

	// Guide update need to run only if simulation is enabled, or if RBF is enabled (since RFB are transfer through guides)
	for (FInstanceData& InstanceData : InstanceDatas)
	{
		if (InstanceData.NeedsMeshUpdate())
		{		
			if (InstanceData.bGlobalDeformationEnable)
			{
				AddHairStrandInitMeshSamplesPass(
					GraphBuilder,
					ShaderMap,
					InstanceData.Instance->Debug.MeshLODIndex,
					InstanceData.MeshDataLOD,
					InstanceData.Instance->Guides.RestRootResource,
					InstanceData.Instance->Guides.DeformedRootResource);
			}
		}
	}

	// Update position offset for each instance (GPU)
	for (FInstanceData& InstanceData : InstanceDatas)
	{
		if (InstanceData.NeedsMeshUpdate())
		{
			check(InstanceData.MeshLODIndex >= 0);

			AddHairStrandUpdatePositionOffsetPass(
				GraphBuilder,
				ShaderMap,
				EHairPositionUpdateType::Guides,
				InstanceData.Instance->RegisteredIndex,
				InstanceData.HairLODIndex,
				InstanceData.Instance->Debug.MeshLODIndex,
				InstanceData.Instance->Guides.DeformedRootResource,
				InstanceData.Instance->Guides.DeformedResource);

			// Add manual transition for the GPU solver as Niagara does not track properly the RDG buffer, and so doesn't issue the correct transitions
			{
				GraphBuilder.UseExternalAccessMode(Register(GraphBuilder, InstanceData.Instance->Guides.DeformedRootResource->LODs[InstanceData.MeshLODIndex].GetDeformedUniqueTrianglePositionBuffer(FHairStrandsDeformedRootResource::FLOD::Current), ERDGImportedBufferFlags::CreateSRV).Buffer, ERHIAccess::SRVMask);

				if (InstanceData.bGlobalDeformationEnable)
				{
					GraphBuilder.UseExternalAccessMode(Register(GraphBuilder, InstanceData.Instance->Guides.DeformedRootResource->LODs[InstanceData.MeshLODIndex].GetDeformedSamplePositionsBuffer(FHairStrandsDeformedRootResource::FLOD::Current), ERDGImportedBufferFlags::CreateSRV).Buffer, ERHIAccess::SRVMask);
					GraphBuilder.UseExternalAccessMode(Register(GraphBuilder, InstanceData.Instance->Guides.DeformedRootResource->LODs[InstanceData.MeshLODIndex].GetMeshSampleWeightsBuffer(FHairStrandsDeformedRootResource::FLOD::Current), ERDGImportedBufferFlags::CreateSRV).Buffer, ERHIAccess::SRVMask);
				}
			}
			GraphBuilder.UseExternalAccessMode(Register(GraphBuilder, InstanceData.Instance->Guides.DeformedResource->GetPositionOffsetBuffer(FHairStrandsDeformedResource::EFrameType::Current), ERDGImportedBufferFlags::CreateSRV).Buffer, ERHIAccess::SRVMask);
		}
	}

	// Update position offset for each instance (CPU)
	for (FInstanceData& InstanceData : InstanceDatas)
	{
		if (InstanceData.MeshLODIndex >= 0 && (InstanceData.BindingType != EHairBindingType::Skinning) && (InstanceData.bSimulationEnable || InstanceData.bDeformationEnable))
		{		
			AddHairStrandUpdatePositionOffsetPass(
				GraphBuilder,
				ShaderMap,
				EHairPositionUpdateType::Guides,
				InstanceData.Instance->RegisteredIndex,
				InstanceData.HairLODIndex,
				InstanceData.Instance->Debug.MeshLODIndex,
				nullptr,
				InstanceData.Instance->Guides.DeformedResource);

			// Add manual transition for the GPU solver as Niagara does not track properly the RDG buffer, and so doesn't issue the correct transitions
			GraphBuilder.UseExternalAccessMode(Register(GraphBuilder, InstanceData.Instance->Guides.DeformedResource->GetPositionOffsetBuffer(FHairStrandsDeformedResource::EFrameType::Current), ERDGImportedBufferFlags::CreateSRV).Buffer, ERHIAccess::SRVMask);
		}
	}

	// Apply deformation from RBF
	for (FInstanceData& InstanceData : InstanceDatas)
	{
		if (
			((InstanceData.Instance->Guides.bIsSimulationEnable || InstanceData.Instance->Guides.bIsDeformationEnable || InstanceData.Instance->Guides.bIsSimulationCacheEnable)) ||
			(!InstanceData.Instance->Guides.bHasGlobalInterpolation && !InstanceData.Instance->Guides.bIsSimulationEnable && !InstanceData.Instance->Guides.bIsDeformationEnable && !InstanceData.Instance->Guides.bIsSimulationCacheEnable) ||
			!IsHairStrandsBindingEnable()) continue;

		FRDGExternalBuffer RawDeformedPositionBuffer = InstanceData.Instance->Guides.DeformedResource->GetBuffer(FHairStrandsDeformedResource::Current);
		FRDGImportedBuffer DeformedPositionBuffer = Register(GraphBuilder, RawDeformedPositionBuffer, ERDGImportedBufferFlags::CreateUAV);

		AddDeformSimHairStrandsPass(
			GraphBuilder,
			ShaderMap,
			InstanceData.Instance->RegisteredIndex,
			InstanceData.Instance->Debug.MeshLODIndex,
			InstanceData.Instance->Guides.RestResource->GetPointCount(),
			InstanceData.Instance->Guides.RestRootResource,
			InstanceData.Instance->Guides.DeformedRootResource,
			RegisterAsSRV(GraphBuilder, InstanceData.Instance->Guides.RestResource->PositionBuffer),
			RegisterAsSRV(GraphBuilder, InstanceData.Instance->Guides.RestResource->PointToCurveBuffer),
			DeformedPositionBuffer,
			InstanceData.Instance->Guides.RestResource->GetPositionOffset(),
			RegisterAsSRV(GraphBuilder, InstanceData.Instance->Guides.DeformedResource->GetPositionOffsetBuffer(FHairStrandsDeformedResource::Current)),
			InstanceData.Instance->Guides.bHasGlobalInterpolation,
			nullptr);
	}

	// Apply deformation from skeletal mesh bones
	for (FInstanceData& InstanceData : InstanceDatas)
	{
		if (InstanceData.ActiveGroomCacheType != EGroomCacheType::None && InstanceData.Instance->Guides.bIsDeformationEnable && InstanceData.Instance->Guides.DeformedResource && InstanceData.Instance->DeformedComponent && (InstanceData.Instance->DeformedSection != INDEX_NONE))
		{
			if (USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(InstanceData.Instance->DeformedComponent))
			{
				if (FSkeletalMeshObject* SkeletalMeshObject = SkeletalMeshComponent->MeshObject)
				{
					const int32 LodIndex = SkeletalMeshObject->GetLOD();
					FSkeletalMeshLODRenderData const* LodRenderData = &SkeletalMeshObject->GetSkeletalMeshRenderData().LODRenderData[LodIndex];
				
					if (LodRenderData->RenderSections.IsValidIndex(InstanceData.Instance->DeformedSection)) 
					{
						FRHIShaderResourceView* BoneBufferSRV = FSkeletalMeshDeformerHelpers::GetBoneBufferForReading(SkeletalMeshObject, LodIndex, InstanceData.Instance->DeformedSection, false);

						// Guides deformation based on the skeletal mesh bones
						FRDGImportedBuffer GuideDeformResource = Register(GraphBuilder, InstanceData.Instance->Guides.DeformedResource->GetBuffer(FHairStrandsDeformedResource::Current), ERDGImportedBufferFlags::CreateUAV);
						AddDeformSimHairStrandsPass(
							GraphBuilder,
							ShaderMap,
							InstanceData.Instance->RegisteredIndex,
							InstanceData.MeshLODIndex,
							InstanceData.Instance->Guides.RestResource->GetPointCount(),
							InstanceData.Instance->Guides.RestRootResource,
							InstanceData.Instance->Guides.DeformedRootResource,
							RegisterAsSRV(GraphBuilder, InstanceData.Instance->Guides.RestResource->PositionBuffer),
							RegisterAsSRV(GraphBuilder, InstanceData.Instance->Guides.RestResource->PointToCurveBuffer),
							GuideDeformResource,
							InstanceData.Instance->Guides.RestResource->GetPositionOffset(),
							RegisterAsSRV(GraphBuilder, InstanceData.Instance->Guides.DeformedResource->GetPositionOffsetBuffer(FHairStrandsDeformedResource::Current)),
							InstanceData.Instance->Guides.bHasGlobalInterpolation,
							BoneBufferSRV);
					}
				}
			}
		}
	}

	// Apply guide cache update
	for (FInstanceData& InstanceData : InstanceDatas)
	{
		if (InstanceData.ActiveGroomCacheType == EGroomCacheType::Guides)
		{
			FScopeLock Lock(InstanceData.Instance->Debug.GroomCacheBuffers->GetCriticalSection());
			FGroomCacheGroupData* GroomCacheData0 = const_cast<FGroomCacheGroupData*>(&InstanceData.Instance->Debug.GroomCacheBuffers->GetCurrentFrameBuffer().GroupsData[InstanceData.Instance->Debug.GroupIndex]);
			FGroomCacheGroupData* GroomCacheData1 = const_cast<FGroomCacheGroupData*>(&InstanceData.Instance->Debug.GroomCacheBuffers->GetNextFrameBuffer().GroupsData[InstanceData.Instance->Debug.GroupIndex]);

			const float InterpolationFactor = InstanceData.Instance->Debug.GroomCacheBuffers->GetInterpolationFactor();

			// Update CPU position offset to GPU
			InstanceData.Instance->Guides.DeformedResource->SetPositionOffset(FHairStrandsDeformedResource::EFrameType::Current, FMath::Lerp(GroomCacheData0->BoundingBox.GetCenter(), GroomCacheData1->BoundingBox.GetCenter(), InterpolationFactor));
			AddHairStrandUpdatePositionOffsetPass(
				GraphBuilder,
				ShaderMap,
				EHairPositionUpdateType::Guides,
				InstanceData.Instance->RegisteredIndex,
				InstanceData.HairLODIndex,
				InstanceData.MeshLODIndex,
				InstanceData.Instance->Guides.DeformedRootResource,
				InstanceData.Instance->Guides.DeformedResource);

			// Pass to upload GroomCache guide positions
			FGroomCacheResources CacheResources0 = CreateGroomCacheBuffer(GraphBuilder, GroomCacheData0->VertexData);
			FGroomCacheResources CacheResources1 = CreateGroomCacheBuffer(GraphBuilder, GroomCacheData1->VertexData);
			AddGroomCacheUpdatePass(
				GraphBuilder,
				ShaderMap,
				InstanceData.Instance->RegisteredIndex,
				InstanceData.Instance->Guides.RestResource->GetPointCount(),
				InterpolationFactor,
				CacheResources0,
				CacheResources1,
				RegisterAsSRV(GraphBuilder, InstanceData.Instance->Guides.RestResource->PositionBuffer),
				RegisterAsSRV(GraphBuilder, InstanceData.Instance->Guides.DeformedResource->GetPositionOffsetBuffer(FHairStrandsDeformedResource::EFrameType::Current)),
				RegisterAsUAV(GraphBuilder, InstanceData.Instance->Guides.DeformedResource->GetBuffer(FHairStrandsDeformedResource::Current)));
		}
	}

}

void AddDrawDebugClusterPass(
	FRDGBuilder& GraphBuilder,
	const FSceneView& View,
	FGlobalShaderMap* ShaderMap,
	FHairTransientResources& TransientResources,
	const FShaderPrintData* ShaderPrintData,
	EGroomViewMode ViewMode,
	FHairStrandClusterData& HairClusterData);

static void RunHairStrandsInterpolation_Strands(
	FRDGBuilder& GraphBuilder,
	FSceneInterface* Scene,
	const TArray<const FSceneView*>& Views, 
	const FSceneView* View,
	const uint32 ViewUniqueID,
	const uint32 TotalRegisteredInstanceCount,
	const FHairStrandsInstances& Instances,
	const FShaderPrintData* ShaderPrintData,
	FHairTransientResources& TransientResources,
	FGlobalShaderMap* ShaderMap)
{
	if (Instances.IsEmpty()) { return; }
	if (!IsHairStrandsEnabled(EHairStrandsShaderType::Strands, Scene->GetShaderPlatform())) { return;  }
	check(IsInRenderingThread());
	check(View);

	DECLARE_GPU_STAT(HairStrandsInterpolation);
	RDG_EVENT_SCOPE(GraphBuilder, "HairInterpolation(Strands)");
	RDG_GPU_STAT_SCOPE(GraphBuilder, HairStrandsInterpolation);

	struct FInstanceRDGResources
	{
		FRDGBufferSRVRef PositionSRV = nullptr;
		FRDGBufferSRVRef PositionOffsetSRV = nullptr;
		FRDGBufferSRVRef TangentSRV = nullptr;

		FRDGBufferSRVRef DeformerPositionSRV = nullptr;
		FRDGBufferUAVRef PositionUAV = nullptr;
		FRDGBufferUAVRef TangentUAV = nullptr;

		FRDGImportedBuffer DeformedPosition;
		FRDGImportedBuffer DeformedPrevPosition;

		FRDGImportedBuffer Raytracing_PositionBuffer;
		FRDGImportedBuffer Raytracing_IndexBuffer;
	};

	struct FInstanceData
	{
		uint32 RegisteredIndex = ~0;
		int32 HairLODIndex = -1;
		int32 MeshLODIndex = -1;
		uint32 ActivePointCount = 0;
		uint32 ActiveCurveCount = 0;
		EGroomCacheType ActiveGroomCacheType = EGroomCacheType::None;
		EHairBindingType BindingType = EHairBindingType::NoneBinding;
		bool bSimulationEnable = false;
		bool bDeformationEnable = false;
		bool bGlobalDeformationEnable = false;
		bool bNeedDeformation = false;
		bool bNeedRaytracing = false;
		bool bNeedRaytracingUpdate = false;
		bool bNeedRaytracingBuild = false;


		FInstanceRDGResources RDGResources;

		FHairStrandsProjectionMeshData::LOD MeshDataLOD;
		FHairGroupInstance* Instance = nullptr;
		FRDGHairStrandsCullingData CullingData;

		bool NeedsMeshUpdate() const
		{
			return 
				MeshLODIndex >= 0 && 
				(BindingType == EHairBindingType::Skinning) && 
				(bGlobalDeformationEnable || bSimulationEnable || bDeformationEnable);
		}
	};

	// Gather all strands instances
	const uint32 ViewRayTracingMask = RHI_RAYTRACING ? (View->Family->EngineShowFlags.PathTracing ? EHairViewRayTracingMask::PathTracing : EHairViewRayTracingMask::RayTracing) : 0u;
	bool bHasAnySimCacheInstances = false;
	bool bHasAnyRenCacheInstances = false;
	bool bHasAnyRaytracingInstances = false;
	TArray<FInstanceData> InstanceDatas;
	InstanceDatas.Reserve(Instances.Num());
	for (FHairStrandsInstance* AbstractInstance : Instances)
	{
		FHairGroupInstance* Instance = static_cast<FHairGroupInstance*>(AbstractInstance);
		if (Instance->GeometryType != EHairGeometryType::Strands)
		{
			continue;
		}	
		check(Instance->HairGroupPublicData);

		FInstanceData& InstanceData = InstanceDatas.AddDefaulted_GetRef();
		InstanceData.RegisteredIndex			= Instance->RegisteredIndex;
		InstanceData.Instance 					= Instance;
		InstanceData.ActivePointCount 			= Instance->HairGroupPublicData->GetActiveStrandsPointCount();
		InstanceData.ActiveCurveCount 			= Instance->HairGroupPublicData->GetActiveStrandsCurveCount();
		InstanceData.ActiveGroomCacheType 		= GetHairInstanceCacheType(Instance);
		InstanceData.BindingType 				= Instance->BindingType;
		InstanceData.HairLODIndex 				= Instance->HairGroupPublicData->LODIndex;
		InstanceData.bSimulationEnable 			= Instance->HairGroupPublicData->IsSimulationEnable(InstanceData.HairLODIndex);
		InstanceData.bDeformationEnable 		= Instance->HairGroupPublicData->bIsDeformationEnable;
		InstanceData.bGlobalDeformationEnable 	= Instance->HairGroupPublicData->IsGlobalInterpolationEnable(InstanceData.HairLODIndex);
		InstanceData.MeshLODIndex				= -1;
		InstanceData.bNeedDeformation 			= Instance->Strands.DeformedResource != nullptr;
		InstanceData.bNeedRaytracing			= false;
		InstanceData.bNeedRaytracingUpdate		= false;
		InstanceData.bNeedRaytracingBuild		= false;

		// Register resources
		FInstanceRDGResources RDGResources;
		if (InstanceData.bNeedDeformation)
		{		
			// Trach on which view the position has been update, so that we can ensure motion vector are coherent
			Instance->Strands.DeformedResource->GetUniqueViewID(FHairStrandsDeformedResource::Current) = ViewUniqueID;
			
			InstanceData.RDGResources.DeformedPosition		= Register(GraphBuilder, Instance->Strands.DeformedResource->GetBuffer(FHairStrandsDeformedResource::Current), ERDGImportedBufferFlags::CreateViews);
			InstanceData.RDGResources.DeformedPrevPosition	= Register(GraphBuilder, Instance->Strands.DeformedResource->GetBuffer(FHairStrandsDeformedResource::Previous), ERDGImportedBufferFlags::CreateViews);

			InstanceData.RDGResources.PositionSRV 			= InstanceData.RDGResources.DeformedPosition.SRV;
			InstanceData.RDGResources.PositionUAV			= InstanceData.RDGResources.DeformedPosition.UAV;
			InstanceData.RDGResources.PositionOffsetSRV 	= RegisterAsSRV(GraphBuilder, Instance->Strands.DeformedResource->GetPositionOffsetBuffer(FHairStrandsDeformedResource::EFrameType::Current));		
			InstanceData.RDGResources.TangentSRV 			= RegisterAsSRV(GraphBuilder, Instance->Strands.DeformedResource->TangentBuffer);
			InstanceData.RDGResources.TangentUAV 			= RegisterAsUAV(GraphBuilder, Instance->Strands.DeformedResource->TangentBuffer);
			InstanceData.RDGResources.DeformerPositionSRV 	= RegisterAsSRV(GraphBuilder, Instance->Strands.DeformedResource->DeformerBuffer);
		}
		else
		{
			InstanceData.RDGResources.PositionSRV 			= RegisterAsSRV(GraphBuilder, Instance->Strands.RestResource->PositionBuffer);
			InstanceData.RDGResources.PositionUAV			= nullptr;
			InstanceData.RDGResources.PositionOffsetSRV 	= RegisterAsSRV(GraphBuilder, Instance->Strands.RestResource->PositionOffsetBuffer);
			InstanceData.RDGResources.TangentSRV 			= nullptr;  
			InstanceData.RDGResources.DeformerPositionSRV 	= nullptr;
		}

		#if RHI_RAYTRACING
		InstanceData.bNeedRaytracing = InstanceData.Instance->Strands.RenRaytracingResource && (InstanceData.Instance->Strands.ViewRayTracingMask & ViewRayTracingMask) != 0;
		if (InstanceData.bNeedRaytracing)
		{
			InstanceData.RDGResources.Raytracing_PositionBuffer = Register(GraphBuilder, InstanceData.Instance->Strands.RenRaytracingResource->PositionBuffer, ERDGImportedBufferFlags::CreateViews);
			InstanceData.RDGResources.Raytracing_IndexBuffer 	= Register(GraphBuilder, InstanceData.Instance->Strands.RenRaytracingResource->IndexBuffer, ERDGImportedBufferFlags::CreateViews);
			bHasAnyRaytracingInstances = true;
		}
		#endif

		if (InstanceData.ActiveGroomCacheType == EGroomCacheType::Guides)  { bHasAnySimCacheInstances = true; }
		if (InstanceData.ActiveGroomCacheType == EGroomCacheType::Strands) { bHasAnyRenCacheInstances = true; }

		// Extract MeshDataLOD and compute MeshLODIndex
		const FCachedGeometry CachedGeometry = GetCacheGeometryForHair(GraphBuilder, Scene, Instance, ShaderMap, true);
		for (int32 SectionIndex = 0; SectionIndex < CachedGeometry.Sections.Num(); ++SectionIndex)
		{
			// Ensure all mesh's sections have the same LOD index
			const int32 SectionLodIndex = CachedGeometry.Sections[SectionIndex].LODIndex;
			if (InstanceData.MeshLODIndex < 0) InstanceData.MeshLODIndex = SectionLodIndex;
			check(InstanceData.MeshLODIndex == SectionLodIndex);

			InstanceData.MeshDataLOD.Sections.Add(ConvertMeshSection(CachedGeometry, SectionIndex));
		}
		Instance->Debug.MeshLODIndex = InstanceData.MeshLODIndex;
		Instance->HairGroupPublicData->VFInput.Strands	= FHairGroupPublicData::FVertexFactoryInput::FStrands();

		if (InstanceData.MeshLODIndex >= 0 && (InstanceData.BindingType == EHairBindingType::Skinning || InstanceData.bGlobalDeformationEnable))
		{
			check(Instance->Strands.HasValidRootData());
			check(Instance->Strands.DeformedRootResource->IsValid(InstanceData.MeshLODIndex));
		}

		if (InstanceData.MeshLODIndex >= 0 && (InstanceData.bSimulationEnable || InstanceData.bDeformationEnable))
		{
			check(Instance->Strands.HasValidData());
		}
	}	

	FRDGExternalAccessQueue ExternalAccessQueue;
	const EGroomViewMode ViewMode = GetGroomViewMode(*View);

	// Early culling
	FHairStrandClusterData ClusterDatas;
	{
		TArray<FRDGBufferSRVRef> Transitions;
		Transitions.Reserve(InstanceDatas.Num());

		// Gather culling jobs
		for (FInstanceData& InstanceData : InstanceDatas)
		{
			if (InstanceData.Instance->Strands.bCullingEnable)
			{
				AddInstanceToClusterData(InstanceData.Instance, ClusterDatas);
				Transitions.Add(RegisterAsSRV(GraphBuilder, InstanceData.Instance->HairGroupPublicData->GetCulledVertexIdBuffer()));
				Transitions.Add(RegisterAsSRV(GraphBuilder, InstanceData.Instance->HairGroupPublicData->GetDrawIndirectRasterComputeBuffer()));
			}
			else
			{
				InstanceData.Instance->HairGroupPublicData->SetCullingResultAvailable(false);
			}
		}

		// Culling pass
		if (Views.Num() > 0 && ClusterDatas.HairGroups.Num() > 0)
		{
			DECLARE_GPU_STAT(HairStrandsClusterCulling);
			RDG_EVENT_SCOPE(GraphBuilder, "HairStrandsClusterCulling");
			TRACE_CPUPROFILER_EVENT_SCOPE(ComputeHairStrandsClustersCulling);
			RDG_GPU_STAT_SCOPE(GraphBuilder, HairStrandsClusterCulling);

			FRDGBufferUAVRef IndirectDispatchArgsUAVWithSkipBarrier = GraphBuilder.CreateUAV(TransientResources.IndirectDispatchArgsBuffer, ERDGUnorderedAccessViewFlags::SkipBarrier);
			AddClusterCullingPass(
				GraphBuilder,
				ShaderMap,
				Views[0],
				ShaderPrintData,
				ClusterDatas,
				IndirectDispatchArgsUAVWithSkipBarrier);

			AddTransitionPass(GraphBuilder, ShaderMap, View->GetShaderPlatform(), Transitions);
		}
	}

	// Update dynamic mesh triangles
	{
		TArray<FRDGBufferSRVRef> Transitions;
		Transitions.Reserve(InstanceDatas.Num());
		for (FInstanceData& InstanceData : InstanceDatas)
		{
			if (InstanceData.MeshLODIndex >= 0 && (InstanceData.BindingType == EHairBindingType::Skinning || InstanceData.bGlobalDeformationEnable))
			{
				AddHairStrandUpdateMeshTrianglesPass(
					GraphBuilder,
					ShaderMap,
					InstanceData.MeshLODIndex,
					InstanceData.MeshDataLOD,
					InstanceData.Instance->Strands.RestRootResource,
					InstanceData.Instance->Strands.DeformedRootResource);
				Transitions.Add(RegisterAsSRV(GraphBuilder, InstanceData.Instance->Strands.DeformedRootResource->LODs[InstanceData.MeshLODIndex].GetDeformedUniqueTrianglePositionBuffer(FHairStrandsDeformedRootResource::FLOD::Current)));
			}
		}
		AddTransitionPass(GraphBuilder, ShaderMap, View->GetShaderPlatform(), Transitions);
	}

	// Update position offset 
	{
		TArray<FRDGBufferSRVRef> Transitions;
		Transitions.Reserve(InstanceDatas.Num());

		// From GPU root triangle
		{
			for (FInstanceData& InstanceData : InstanceDatas)
			{
				if (InstanceData.MeshLODIndex >= 0 && (InstanceData.BindingType == EHairBindingType::Skinning || InstanceData.bGlobalDeformationEnable))
				{
					AddHairStrandUpdatePositionOffsetPass(
						GraphBuilder,
						ShaderMap,
						EHairPositionUpdateType::Strands,
						InstanceData.Instance->RegisteredIndex,
						InstanceData.HairLODIndex,
						InstanceData.MeshLODIndex,
						InstanceData.Instance->Strands.DeformedRootResource,
						InstanceData.Instance->Strands.DeformedResource);
					Transitions.Add(RegisterAsSRV(GraphBuilder, InstanceData.Instance->Strands.DeformedResource->GetPositionOffsetBuffer(FHairStrandsDeformedResource::EFrameType::Current)));
				}
			}
		}
	
		// From CPU AABB
		for (FInstanceData& InstanceData : InstanceDatas)
		{
			if (InstanceData.MeshLODIndex >= 0 && (InstanceData.bSimulationEnable || InstanceData.bDeformationEnable))
			{
				AddHairStrandUpdatePositionOffsetPass(
					GraphBuilder,
					ShaderMap,
					EHairPositionUpdateType::Strands,
					InstanceData.Instance->RegisteredIndex,
					InstanceData.HairLODIndex,
					InstanceData.MeshLODIndex,
					nullptr,
					InstanceData.Instance->Strands.DeformedResource);
				Transitions.Add(RegisterAsSRV(GraphBuilder, InstanceData.Instance->Strands.DeformedResource->GetPositionOffsetBuffer(FHairStrandsDeformedResource::EFrameType::Current)));
			}
		}
		
		// From Sim Cache
		if (bHasAnySimCacheInstances)
		{
			for (FInstanceData& InstanceData : InstanceDatas)
			{
				if (InstanceData.ActiveGroomCacheType == EGroomCacheType::Guides && InstanceData.bNeedDeformation)
				{
					// Apply the same offset to the render strands for proper rendering
					const FVector OffsetPosition = InstanceData.Instance->Guides.DeformedResource->GetPositionOffset(FHairStrandsDeformedResource::EFrameType::Current);
					InstanceData.Instance->Strands.DeformedResource->SetPositionOffset(FHairStrandsDeformedResource::EFrameType::Current, OffsetPosition);
		
					AddHairStrandUpdatePositionOffsetPass(
						GraphBuilder,
						ShaderMap,
						EHairPositionUpdateType::Strands,
						InstanceData.Instance->RegisteredIndex,
						0,
						InstanceData.MeshLODIndex,
						InstanceData.Instance->Strands.DeformedRootResource,
						InstanceData.Instance->Strands.DeformedResource);
					Transitions.Add(RegisterAsSRV(GraphBuilder, InstanceData.Instance->Strands.DeformedResource->GetPositionOffsetBuffer(FHairStrandsDeformedResource::EFrameType::Current)));
				}
			}
		}

		AddTransitionPass(GraphBuilder, ShaderMap, View->GetShaderPlatform(), Transitions);
	}


	// Clear cluster AABBs (used optionally  for voxel allocation & for culling)
	{
		uint32 TotalClusterAABBCount = 0;
		TransientResources.ClusterAABBOffetAndCounts.Init(FUintVector2::ZeroValue, TotalRegisteredInstanceCount); // Use TotalRegisteredInstanceCount because this buffer is indexed by RegisteredIndex
		for (FInstanceData& InstanceData : InstanceDatas)
		{
			if (InstanceData.bNeedDeformation)
			{
				TransientResources.ClusterAABBOffetAndCounts[InstanceData.RegisteredIndex].X = TotalClusterAABBCount;
				TransientResources.ClusterAABBOffetAndCounts[InstanceData.RegisteredIndex].Y = InstanceData.Instance->HairGroupPublicData->ClusterCount;
				TotalClusterAABBCount += InstanceData.Instance->HairGroupPublicData->ClusterCount;
			}
		}
		TotalClusterAABBCount = FMath::Max(TotalClusterAABBCount, 1u);
		TransientResources.ClusterAABBBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(4, 6u * TotalClusterAABBCount), TEXT("Hair.Transient.ClusterAABBs"));
		TransientResources.ClusterAABBSRV = GraphBuilder.CreateSRV(TransientResources.ClusterAABBBuffer, PF_R32_SINT);

		FRDGBufferUAVRef ClusterAABBUAVSkipBarrier = GraphBuilder.CreateUAV(TransientResources.ClusterAABBBuffer, PF_R32_SINT, ERDGUnorderedAccessViewFlags::SkipBarrier);
		FRDGBufferUAVRef GroupAABBUAVSkipBarrier = GraphBuilder.CreateUAV(TransientResources.GroupAABBBuffer, PF_R32_SINT, ERDGUnorderedAccessViewFlags::SkipBarrier);
		AddClearAABBPass(GraphBuilder, ShaderMap, TotalClusterAABBCount, ClusterAABBUAVSkipBarrier);
		AddClearAABBPass(GraphBuilder, ShaderMap, TotalRegisteredInstanceCount, GroupAABBUAVSkipBarrier);

		for (FInstanceData& InstanceData : InstanceDatas)
		{
			InstanceData.CullingData = ImportCullingData(GraphBuilder, InstanceData.Instance->HairGroupPublicData);
		}
	}

	{
		TArray<FRDGBufferSRVRef> Transitions;
		Transitions.Reserve(InstanceDatas.Num());
		for (FInstanceData& InstanceData : InstanceDatas)
		{
			if (InstanceData.ActiveGroomCacheType != EGroomCacheType::Strands && InstanceData.bNeedDeformation)
			{
				const bool bValidGuide		= InstanceData.bNeedDeformation && (InstanceData.bSimulationEnable || InstanceData.bGlobalDeformationEnable || InstanceData.bDeformationEnable || InstanceData.Instance->Guides.bIsSimulationCacheEnable);// || (WITH_EDITOR && PatchMode == EHairPatchAttribute::GuideInflucence);
				const bool bUseSingleGuide	= InstanceData.bNeedDeformation && bValidGuide && InstanceData.Instance->Strands.InterpolationResource->UseSingleGuide();
				const bool bHasSkinning		= InstanceData.bNeedDeformation && InstanceData.BindingType == EHairBindingType::Skinning;
	
				AddHairStrandsInterpolationPass(
					GraphBuilder,
					ShaderMap,
					ShaderPrintData,
					InstanceData.Instance,
					InstanceData.ActivePointCount,
					InstanceData.MeshLODIndex,
					InstanceData.Instance->Strands.Modifier.HairLengthScale,
					InstanceData.Instance->Strands.HairInterpolationType,
					EHairGeometryType::Strands,
					InstanceData.CullingData,
					InstanceData.Instance->Strands.RestResource->GetPositionOffset(),
					bValidGuide ? InstanceData.Instance->Guides.RestResource->GetPositionOffset() : FVector::ZeroVector,
					InstanceData.RDGResources.PositionOffsetSRV,
					bValidGuide ? RegisterAsSRV(GraphBuilder, InstanceData.Instance->Guides.DeformedResource->GetPositionOffsetBuffer(FHairStrandsDeformedResource::Current)) : nullptr,
					bHasSkinning ? InstanceData.Instance->Strands.RestRootResource : nullptr,
					bHasSkinning && bValidGuide ? InstanceData.Instance->Guides.RestRootResource : nullptr,
					bHasSkinning ? InstanceData.Instance->Strands.DeformedRootResource : nullptr,
					bHasSkinning && bValidGuide ? InstanceData.Instance->Guides.DeformedRootResource : nullptr,
					RegisterAsSRV(GraphBuilder, InstanceData.Instance->Strands.RestResource->PositionBuffer),
					RegisterAsSRV(GraphBuilder, InstanceData.Instance->Strands.RestResource->PointToCurveBuffer),
					bUseSingleGuide,
					bValidGuide ? RegisterAsSRV(GraphBuilder, InstanceData.Instance->Strands.InterpolationResource->InterpolationBuffer) : nullptr,
					bValidGuide ? RegisterAsSRV(GraphBuilder, InstanceData.Instance->Guides.RestResource->PositionBuffer) : nullptr,
					bValidGuide ? RegisterAsSRV(GraphBuilder, InstanceData.Instance->Guides.DeformedResource->GetBuffer(FHairStrandsDeformedResource::Current)) : nullptr,
					bValidGuide ? RegisterAsSRV(GraphBuilder, InstanceData.Instance->Strands.InterpolationResource->SimRootPointIndexBuffer) : nullptr,
					bValidGuide ? RegisterAsSRV(GraphBuilder, InstanceData.Instance->Guides.RestResource->PointToCurveBuffer) : nullptr,
					InstanceData.RDGResources.DeformerPositionSRV,
					InstanceData.RDGResources.PositionUAV,
					FHairStrandsDeformedRootResource::FLOD::Current);
	
				Transitions.Add(InstanceData.RDGResources.PositionSRV);

				GraphBuilder.SetBufferAccessFinal(InstanceData.RDGResources.DeformedPosition.Buffer, ERHIAccess::SRVMask);
			}
		}

		AddTransitionPass(GraphBuilder, ShaderMap, View->GetShaderPlatform(), Transitions);
	}

	// Previous Position
	// * Transfer prev. position
	// * Or recompute interpolation pass for previous positions if needed
	for (FInstanceData& InstanceData : InstanceDatas)
	{
		if (InstanceData.ActiveGroomCacheType != EGroomCacheType::Strands && InstanceData.bNeedDeformation)
		{
			const bool bTransferPrevPosition =
				InstanceData.Instance->HairGroupPublicData->VFInput.bHasLODSwitch &&
				IsHairStrandsTransferPositionOnLODChange();
			if (bTransferPrevPosition)
			{
				AddTransferPositionPass(GraphBuilder, ShaderMap, InstanceData.ActivePointCount, InstanceData.RDGResources.DeformedPosition.SRV, InstanceData.RDGResources.DeformedPrevPosition.UAV);
				GraphBuilder.SetBufferAccessFinal(InstanceData.RDGResources.DeformedPrevPosition.Buffer, ERHIAccess::SRVMask);
			}
		}
	}

	// Render Strands cache update
	if (bHasAnyRenCacheInstances)
	{
		for (FInstanceData& InstanceData : InstanceDatas)
		{
			if (InstanceData.ActiveGroomCacheType == EGroomCacheType::Strands)
			{
				FScopeLock Lock(InstanceData.Instance->Debug.GroomCacheBuffers->GetCriticalSection());
				
				FGroomCacheGroupData* GroomCacheData0 = const_cast<FGroomCacheGroupData*>(&InstanceData.Instance->Debug.GroomCacheBuffers->GetCurrentFrameBuffer().GroupsData[InstanceData.Instance->Debug.GroupIndex]);
				FGroomCacheGroupData* GroomCacheData1 = const_cast<FGroomCacheGroupData*>(&InstanceData.Instance->Debug.GroomCacheBuffers->GetNextFrameBuffer().GroupsData[InstanceData.Instance->Debug.GroupIndex]);
				const float InterpolationFactor = InstanceData.Instance->Debug.GroomCacheBuffers->GetInterpolationFactor();
				
				FGroomCacheResources CacheResources0 = CreateGroomCacheBuffer(GraphBuilder, GroomCacheData0->VertexData);
				FGroomCacheResources CacheResources1 = CreateGroomCacheBuffer(GraphBuilder, GroomCacheData1->VertexData);
				
				// Update position offset
				{
					const FVector OffsetPosition = FMath::Lerp(GroomCacheData0->BoundingBox.GetCenter(), GroomCacheData1->BoundingBox.GetCenter(), InterpolationFactor);
					InstanceData.Instance->Strands.DeformedResource->SetPositionOffset(FHairStrandsDeformedResource::EFrameType::Current, OffsetPosition);
				}
				
				// Update max radius
				if (GroomCacheData0->VertexData.PointsRadius.Num() > 0)
				{
					InstanceData.Instance->Strands.Modifier.HairWidth = FMath::Lerp(GroomCacheData0->StrandData.MaxRadius, GroomCacheData1->StrandData.MaxRadius, InterpolationFactor) * 2.0f;
				}
				
				AddHairStrandUpdatePositionOffsetPass(
					GraphBuilder,
					ShaderMap,
					EHairPositionUpdateType::Strands,
					InstanceData.Instance->RegisteredIndex,
					0,
					InstanceData.MeshLODIndex,
					InstanceData.Instance->Strands.DeformedRootResource,
					InstanceData.Instance->Strands.DeformedResource);
				
				// Pass to upload GroomCache strands positions
				AddGroomCacheUpdatePass(
					GraphBuilder,
					ShaderMap,
					InstanceData.Instance->RegisteredIndex,
					InstanceData.ActivePointCount,
					InterpolationFactor,
					CacheResources0,
					CacheResources1,
					RegisterAsSRV(GraphBuilder, InstanceData.Instance->Strands.RestResource->PositionBuffer),
					RegisterAsSRV(GraphBuilder, InstanceData.Instance->Strands.DeformedResource->GetPositionOffsetBuffer(FHairStrandsDeformedResource::EFrameType::Current)),
					InstanceData.RDGResources.PositionUAV);
			}
		}
	}

	// Update tangent data based on the deformed positions
	for (FInstanceData& InstanceData : InstanceDatas)
	{
		if (InstanceData.bNeedDeformation)
		{
			AddHairTangentPass(
				GraphBuilder,
				ShaderMap,
				InstanceData.ActivePointCount,
				InstanceData.Instance->HairGroupPublicData,
				InstanceData.RDGResources.PositionSRV,
				InstanceData.RDGResources.TangentUAV);
		}
		else
		{
			InstanceData.Instance->Strands.RestResource->GetTangentBuffer(GraphBuilder, ShaderMap, InstanceData.ActivePointCount, InstanceData.ActiveCurveCount);
		}
	}

	// Patch attribute for debug visualization (guide influence or clusters visualization)
	const bool bNeedPatchPass = ViewMode == EGroomViewMode::RenderHairStrands || ViewMode == EGroomViewMode::Cluster || ViewMode == EGroomViewMode::ClusterAABB;
	if (bNeedPatchPass)
	{
		for (FInstanceData& InstanceData : InstanceDatas)
		{
			EHairPatchAttribute PatchMode = EHairPatchAttribute::None;
			if (ViewMode == EGroomViewMode::RenderHairStrands && InstanceData.bNeedDeformation)	{ PatchMode = EHairPatchAttribute::GuideInflucence; }
			if (ViewMode == EGroomViewMode::Cluster || ViewMode == EGroomViewMode::ClusterAABB)	{ PatchMode = EHairPatchAttribute::ClusterInfluence; }

			if (PatchMode != EHairPatchAttribute::None)
			{
				// Create an debug buffer for storing cluster visualization data. This is only used for debug purpose, hence only enable in editor build.
				// Special case for debug mode were the attribute buffer is patch with some custom data to show hair properties (strands belonging to the same cluster, ...)
				if (InstanceData.Instance->Strands.DebugCurveAttributeBuffer.Buffer == nullptr)
				{
					CreateHairStrandsDebugAttributeBuffer(GraphBuilder, &InstanceData.Instance->Strands.DebugCurveAttributeBuffer, InstanceData.Instance->Strands.Data->GetCurveAttributeSizeInBytes(), FName(InstanceData.Instance->Debug.MeshComponentName));
				}
				FRDGImportedBuffer OutRenCurveAttributeBuffer = Register(GraphBuilder, InstanceData.Instance->Strands.DebugCurveAttributeBuffer, ERDGImportedBufferFlags::CreateUAV);
			
				const bool bValidGuide = InstanceData.bNeedDeformation && (InstanceData.bSimulationEnable || InstanceData.bGlobalDeformationEnable || InstanceData.bDeformationEnable || InstanceData.Instance->Guides.bIsSimulationCacheEnable);// || (WITH_EDITOR && PatchMode == EHairPatchAttribute::GuideInflucence);
				const bool bUseSingleGuide	= InstanceData.bNeedDeformation && bValidGuide && InstanceData.Instance->Strands.InterpolationResource->UseSingleGuide();

				check(InstanceData.Instance->Strands.Data);
				AddPatchAttributePass(
					GraphBuilder,
					ShaderMap,
					InstanceData.ActiveCurveCount,
					PatchMode,
					bValidGuide,
					bUseSingleGuide,
					*InstanceData.Instance->Strands.Data,
					Register(GraphBuilder, InstanceData.Instance->Strands.RestResource->CurveAttributeBuffer, ERDGImportedBufferFlags::CreateSRV).Buffer,
					RegisterAsSRV(GraphBuilder, InstanceData.Instance->Strands.RestResource->CurveBuffer),
					RegisterAsSRV(GraphBuilder, InstanceData.Instance->Strands.ClusterResource->CurveToClusterIdBuffer),
					bValidGuide ? RegisterAsSRV(GraphBuilder, InstanceData.Instance->Strands.InterpolationResource->InterpolationBuffer) : nullptr,
					OutRenCurveAttributeBuffer);
			}
		}
	}

	// Update the VF input with the update resources
	for (FInstanceData& InstanceData : InstanceDatas)
	{
		InstanceData.Instance->HairGroupPublicData->VFInput = InternalComputeHairStrandsVertexInputData(&GraphBuilder, InstanceData.Instance, ViewMode);
	}

	// 3. Compute cluster AABBs (used for LODing and voxelization)
	{
		FRDGBufferUAVRef GroupAABUAVSkipBarrier = GraphBuilder.CreateUAV(TransientResources.GroupAABBBuffer, PF_R32_SINT, ERDGUnorderedAccessViewFlags::SkipBarrier);
		FRDGBufferUAVRef ClusterAABBUAVSkipBarrier = GraphBuilder.CreateUAV(TransientResources.ClusterAABBBuffer, PF_R32_SINT, ERDGUnorderedAccessViewFlags::SkipBarrier);

		const FVector& TranslatedWorldOffset = View->ViewMatrices.GetPreViewTranslation();
		for (FInstanceData& InstanceData : InstanceDatas)
		{
			// Optim: If an instance does not voxelize it's data, then there is no need for having valid AABB
			bool bNeedGPUAABB = InstanceData.Instance->Strands.Modifier.bSupportVoxelization && InstanceData.Instance->bCastShadow;
			const EHairAABBUpdateType UpdateType = InstanceData.bNeedDeformation ? EHairAABBUpdateType::UpdateClusterAABB : EHairAABBUpdateType::UpdateGroupAABB;
				
			FHairStrandClusterData::FHairGroup* HairGroupCluster = nullptr;
			if (InstanceData.CullingData.bCullingResultAvailable)
			{
				// Sanity check
				check(InstanceData.Instance->Strands.bCullingEnable);
				HairGroupCluster = &ClusterDatas.HairGroups[InstanceData.Instance->HairGroupPublicData->ClusterDataIndex];
				if (!HairGroupCluster->bVisible)
				{
					bNeedGPUAABB = false;
				}
			}
				
			if (bNeedGPUAABB)
			{
				FRDGImportedBuffer Strands_CulledVertexCount = Register(GraphBuilder, InstanceData.Instance->HairGroupPublicData->GetDrawIndirectRasterComputeBuffer(), ERDGImportedBufferFlags::CreateSRV);
				AddHairClusterAABBPass(
					GraphBuilder,
					ShaderMap,
					InstanceData.ActiveCurveCount,
					TranslatedWorldOffset,
					ShaderPrintData,
					UpdateType,
					InstanceData.Instance,
					InstanceData.RDGResources.PositionOffsetSRV,
					HairGroupCluster,
					TransientResources.GetClusterOffset(InstanceData.Instance->RegisteredIndex),
					TransientResources.GetClusterCount(InstanceData.Instance->RegisteredIndex),
					InstanceData.RDGResources.PositionSRV,
					Strands_CulledVertexCount.SRV,
					ClusterAABBUAVSkipBarrier,
					GroupAABUAVSkipBarrier);
	
				TransientResources.bIsGroupAABBValid[InstanceData.RegisteredIndex] = true;
			}
		}

		// Run cluster debug view here (instead of GroomDebug.h/.cpp, as we need to have the (transient) cluster data 
		if (ViewMode == EGroomViewMode::Cluster || ViewMode == EGroomViewMode::ClusterAABB)
		{
			AddDrawDebugClusterPass(GraphBuilder, *View, ShaderMap, TransientResources, ShaderPrintData, ViewMode, ClusterDatas);
		}
	}

	#if RHI_RAYTRACING
	if (bHasAnyRaytracingInstances)
	{
		const uint32 ProceduralSplits = GetHairRaytracingProceduralSplits();

		// 1. Update raytracing geometry (update only if the view mask and the RT geometry mask match)
		for (FInstanceData& InstanceData : InstanceDatas)
		{
			if (InstanceData.bNeedRaytracing)
			{
				const float CLODScale = InstanceData.Instance->HairGroupPublicData->ContinuousLODCoverageScale;
				const float HairRadiusRT = InstanceData.Instance->HairGroupPublicData->VFInput.Strands.Common.RaytracingRadiusScale * InstanceData.Instance->HairGroupPublicData->VFInput.Strands.Common.Radius * CLODScale;
				const float HairRootScaleRT = InstanceData.Instance->HairGroupPublicData->VFInput.Strands.Common.RootScale;
				const float HairTipScaleRT = InstanceData.Instance->HairGroupPublicData->VFInput.Strands.Common.TipScale;

				InstanceData.bNeedRaytracingUpdate = InstanceData.Instance->Strands.DeformedResource != nullptr ||
					InstanceData.Instance->Strands.CachedHairScaledRadius != HairRadiusRT ||
					InstanceData.Instance->Strands.CachedHairRootScale != HairRootScaleRT ||
					InstanceData.Instance->Strands.CachedHairTipScale != HairTipScaleRT ||
					InstanceData.Instance->Strands.CachedProceduralSplits != ProceduralSplits;
				InstanceData.bNeedRaytracingBuild = !InstanceData.Instance->Strands.RenRaytracingResource->bIsRTGeometryInitialized;
				if (InstanceData.bNeedRaytracingBuild || InstanceData.bNeedRaytracingUpdate)
				{
					const bool bProceduralPrimitive = InstanceData.Instance->Strands.RenRaytracingResource->bProceduralPrimitive;
					AddGenerateRaytracingGeometryPass(
						GraphBuilder,
						ShaderMap,
						ShaderPrintData,
						InstanceData.Instance->RegisteredIndex,
						InstanceData.ActivePointCount,
						bProceduralPrimitive,
						ProceduralSplits,
						HairRadiusRT,
						HairRootScaleRT,
						HairTipScaleRT,
						InstanceData.RDGResources.PositionOffsetSRV,
						InstanceData.CullingData,
						InstanceData.RDGResources.PositionSRV,
						InstanceData.RDGResources.TangentSRV,
						InstanceData.RDGResources.Raytracing_PositionBuffer.UAV,
						InstanceData.RDGResources.Raytracing_IndexBuffer.UAV);
			
					InstanceData.Instance->Strands.CachedHairScaledRadius = HairRadiusRT;
					InstanceData.Instance->Strands.CachedHairRootScale = HairRootScaleRT;
					InstanceData.Instance->Strands.CachedHairTipScale = HairTipScaleRT;
				}
			}
		}

		// 2. Build/update acceleration structure
		for (FInstanceData& InstanceData : InstanceDatas)
		{
			if (InstanceData.bNeedRaytracing && (InstanceData.bNeedRaytracingBuild || InstanceData.bNeedRaytracingUpdate))
			{
				AddBuildStrandsAccelerationStructurePass(
					GraphBuilder,
					InstanceData.Instance,
					ProceduralSplits,
					InstanceData.bNeedRaytracingUpdate,
					InstanceData.RDGResources.Raytracing_PositionBuffer.Buffer,
					InstanceData.RDGResources.Raytracing_IndexBuffer.Buffer);
			}
		}
	}
	#endif

	if (ViewMode == EGroomViewMode::SimHairStrands)
	{
		for (FInstanceData& InstanceData : InstanceDatas)
		{
			if (InstanceData.Instance->Guides.DeformedResource)
			{
				// Disable culling when drawing only guides, as the culling output has been computed for the strands, not for the guides.
				InstanceData.Instance->HairGroupPublicData->SetCullingResultAvailable(false);
					
				AddHairTangentPass(
					GraphBuilder,
					ShaderMap,
					InstanceData.Instance->Guides.RestResource->GetPointCount(),
					InstanceData.Instance->HairGroupPublicData,
					RegisterAsSRV(GraphBuilder, InstanceData.Instance->Guides.DeformedResource->GetBuffer(FHairStrandsDeformedResource::Current)),
					RegisterAsUAV(GraphBuilder, InstanceData.Instance->Guides.DeformedResource->TangentBuffer));					
			}
			InstanceData.Instance->HairGroupPublicData->VFInput = InternalComputeHairStrandsVertexInputData(&GraphBuilder, InstanceData.Instance, ViewMode);
		}
	}

	// Sanity check
	for (FInstanceData& InstanceData : InstanceDatas)
	{
		check(InstanceData.Instance->HairGroupPublicData->VFInput.Strands.PositionBuffer.Buffer);
		InstanceData.Instance->Strands.UniformBuffer.UpdateUniformBufferImmediate(GraphBuilder.RHICmdList, InstanceData.Instance->GetHairStandsUniformShaderParameters(ViewMode));
		InstanceData.Instance->HairGroupPublicData->VFInput.GeometryType = EHairGeometryType::Strands;
		InstanceData.Instance->HairGroupPublicData->VFInput.LocalToWorldTransform = InstanceData.Instance->GetCurrentLocalToWorld();
		InstanceData.Instance->HairGroupPublicData->bSupportVoxelization = InstanceData.Instance->Strands.Modifier.bSupportVoxelization && InstanceData.Instance->bCastShadow;
	}

	ExternalAccessQueue.Submit(GraphBuilder);
}

static void RunHairStrandsInterpolation_Cards(
	FRDGBuilder& GraphBuilder,
	FSceneInterface* Scene,
	const TArray<const FSceneView*>& Views, 
	const FSceneView* View,
	const uint32 ViewUniqueID,
	const FHairStrandsInstances& Instances,
	const FShaderPrintData* ShaderPrintData,
	FGlobalShaderMap* ShaderMap)
{
	if (Instances.IsEmpty()) { return; }

	check(IsInRenderingThread());

	DECLARE_GPU_STAT(HairCardsInterpolation);
	RDG_EVENT_SCOPE(GraphBuilder, "HairCardsInterpolation");
	RDG_GPU_STAT_SCOPE(GraphBuilder, HairCardsInterpolation);

	RunInternalHairInterpolation(
		GraphBuilder,
		Scene,
		View,
		ViewUniqueID,
		Instances,
		ShaderPrintData,
		ShaderMap,
		EHairInterpolationPassType::CardsAndMeshes,
		nullptr);
}

// Return the LOD which should be used for a given screen size and LOD bias value
// This function is mirrored in HairStrandsClusterCommon.ush
static float GetHairInstanceLODIndex(const TArray<float>& InLODScreenSizes, float InScreenSize, float InLODBias)
{
	const uint32 LODCount = InLODScreenSizes.Num();
	check(LODCount > 0);

	float OutLOD = 0;
	if (LODCount > 1 && InScreenSize < InLODScreenSizes[0])
	{
		for (uint32 LODIt = 1; LODIt < LODCount; ++LODIt)
		{
			if (InScreenSize >= InLODScreenSizes[LODIt])
			{
				uint32 PrevLODIt = LODIt - 1;

				const float S_Delta = abs(InLODScreenSizes[PrevLODIt] - InLODScreenSizes[LODIt]);
				const float S = S_Delta > 0 ? FMath::Clamp(FMath::Abs(InScreenSize - InLODScreenSizes[LODIt]) / S_Delta, 0.f, 1.f) : 0;
				OutLOD = PrevLODIt + (1 - S);
				break;
			}
			else if (LODIt == LODCount - 1)
			{
				OutLOD = LODIt;
			}
		}
	}

	if (InLODBias != 0)
	{
		OutLOD = FMath::Clamp(OutLOD + InLODBias, 0.f, float(LODCount - 1));
	}
	return OutLOD;
}


static EHairGeometryType ConvertLODGeometryType(EHairGeometryType Type, bool InbUseCards, EShaderPlatform Platform)
{
	// Force cards only if it is enabled or fallback on cards if strands are disabled
	InbUseCards = (InbUseCards || !IsHairStrandsEnabled(EHairStrandsShaderType::Strands, Platform)) && IsHairStrandsEnabled(EHairStrandsShaderType::Cards, Platform);

	switch (Type)
	{
	case EHairGeometryType::Strands: return IsHairStrandsEnabled(EHairStrandsShaderType::Strands, Platform) ? (InbUseCards ? EHairGeometryType::Cards : EHairGeometryType::Strands) : (InbUseCards ? EHairGeometryType::Cards : EHairGeometryType::NoneGeometry);
	case EHairGeometryType::Cards:   return IsHairStrandsEnabled(EHairStrandsShaderType::Cards, Platform) ? EHairGeometryType::Cards : EHairGeometryType::NoneGeometry;
	case EHairGeometryType::Meshes:  return IsHairStrandsEnabled(EHairStrandsShaderType::Meshes, Platform) ? EHairGeometryType::Meshes : EHairGeometryType::NoneGeometry;
	}
	return EHairGeometryType::NoneGeometry;
}

static void RunHairBufferSwap(const FHairStrandsInstances& Instances, const TArray<const FSceneView*> Views)
{
	EShaderPlatform ShaderPlatform = EShaderPlatform::SP_NumPlatforms;
	if (Views.Num() > 0)
	{
		ShaderPlatform = Views[0]->GetShaderPlatform();
	}

	for (FHairStrandsInstance* AbstractInstance : Instances)
	{
		FHairGroupInstance* Instance = static_cast<FHairGroupInstance*>(AbstractInstance);

		int32 MeshLODIndex = -1;
		check(Instance);
		if (!Instance)
			continue;

		check(Instance->HairGroupPublicData);

		// Swap current/previous buffer for all LODs
		const bool bIsPaused = Views[0]->Family->bWorldIsPaused;
		if (!bIsPaused)
		{
			if (Instance->Guides.DeformedResource) { Instance->Guides.DeformedResource->SwapBuffer(); }
			if (Instance->Strands.DeformedResource) { Instance->Strands.DeformedResource->SwapBuffer(); }

			// Double buffering is disabled by default unless the read-only cvar r.HairStrands.ContinuousDecimationReordering is set
			if (IsHairStrandContinuousDecimationReorderingEnabled())
			{
				if (Instance->Guides.DeformedRootResource) { Instance->Guides.DeformedRootResource->SwapBuffer(); }
				if (Instance->Strands.DeformedRootResource) { Instance->Strands.DeformedRootResource->SwapBuffer(); }
			}

			Instance->Debug.LastFrameIndex = Views[0]->Family->FrameNumber;
		}
	}
}

static void AddCopyHairStrandsPositionPass(
	FRDGBuilder& GraphBuilder,
	FGlobalShaderMap* ShaderMap,
	FHairStrandsRestResource& RestResources,
	FHairStrandsDeformedResource& DeformedResources)
{
	FRDGBufferRef RestBuffer = Register(GraphBuilder, RestResources.PositionBuffer, ERDGImportedBufferFlags::None).Buffer;
	FRDGBufferRef Deformed0Buffer = Register(GraphBuilder, DeformedResources.DeformedPositionBuffer[0], ERDGImportedBufferFlags::None).Buffer;
	FRDGBufferRef Deformed1Buffer = Register(GraphBuilder, DeformedResources.DeformedPositionBuffer[1], ERDGImportedBufferFlags::None).Buffer;
	AddCopyBufferPass(GraphBuilder, Deformed0Buffer, RestBuffer);
	AddCopyBufferPass(GraphBuilder, Deformed1Buffer, RestBuffer);
}

#if RHI_RAYTRACING
static void AllocateRaytracingResources(FHairGroupInstance* Instance)
{
	if (IsHairRayTracingEnabled() && !Instance->Strands.RenRaytracingResource)
	{
		check(Instance->Strands.Data);

#if RHI_ENABLE_RESOURCE_INFO
		FName OwnerName(FString::Printf(TEXT("%s [LOD%d]"), *Instance->Debug.MeshComponentName, Instance->Debug.MeshLODIndex));
#else
		FName OwnerName = NAME_None;
#endif

		// Allocate dynamic raytracing resources (owned by the groom component/instance)
		FHairResourceName ResourceName(FName(Instance->Debug.GroomAssetName), Instance->Debug.GroupIndex);
		Instance->Strands.RenRaytracingResource		 = new FHairStrandsRaytracingResource(*Instance->Strands.Data, ResourceName, OwnerName);
		Instance->Strands.RenRaytracingResourceOwned = true;
	}
	Instance->Strands.ViewRayTracingMask |= EHairViewRayTracingMask::PathTracing;
}
#endif

void AddHairStreamingRequest(FHairGroupInstance* Instance, int32 InLODIndex)
{
	if (Instance && InLODIndex >= 0)
	{
		check(Instance->HairGroupPublicData);

		// Hypothesis: the mesh LOD will be the same as the hair LOD
		const int32 MeshLODIndex = InLODIndex;
		
		// Insure that MinLOD is necessary taken into account if a force LOD is request (i.e., LODIndex>=0). If a Force LOD 
		// is not resquested (i.e., LODIndex<0), the MinLOD is applied after ViewLODIndex has been determined in the codeblock below
		const float MinLOD = FMath::Max(0, GHairStrandsMinLOD);
		float LODIndex = FMath::Max(InLODIndex, MinLOD);

		const TArray<EHairGeometryType>& LODGeometryTypes = Instance->HairGroupPublicData->GetLODGeometryTypes();
		const TArray<bool>& LODVisibilities = Instance->HairGroupPublicData->GetLODVisibilities();
		const int32 LODCount = LODVisibilities.Num();

		LODIndex = FMath::Clamp(LODIndex, 0.f, float(LODCount - 1));

		const int32 IntLODIndex = FMath::Clamp(FMath::FloorToInt(LODIndex), 0, LODCount - 1);
		const bool bIsVisible = LODVisibilities[IntLODIndex];
		const bool bForceCards = GHairStrands_UseCards > 0 || Instance->bForceCards; // todo
		EHairGeometryType GeometryType = ConvertLODGeometryType(LODGeometryTypes[IntLODIndex], bForceCards, GMaxRHIShaderPlatform);

		if (GeometryType == EHairGeometryType::Meshes)
		{
			if (!Instance->Meshes.IsValid(IntLODIndex))
			{
				GeometryType = EHairGeometryType::NoneGeometry;
			}
		}
		else if (GeometryType == EHairGeometryType::Cards)
		{
			if (!Instance->Cards.IsValid(IntLODIndex))
			{
				GeometryType = EHairGeometryType::NoneGeometry;
			}
		}
		else if (GeometryType == EHairGeometryType::Strands)
		{
			if (!Instance->Strands.IsValid())
			{
				GeometryType = EHairGeometryType::NoneGeometry;
			}
		}

		if (!bIsVisible)
		{
			GeometryType = EHairGeometryType::NoneGeometry;
		}

		const bool bSimulationEnable			= Instance->HairGroupPublicData->IsSimulationEnable(LODIndex);
		const bool bDeformationEnable			= Instance->HairGroupPublicData->bIsDeformationEnable;
		const bool bGlobalInterpolationEnable	= Instance->HairGroupPublicData->IsGlobalInterpolationEnable(LODIndex);
		const bool bSimulationCacheEnable		= Instance->HairGroupPublicData->bIsSimulationCacheEnable;
		const bool bLODNeedsGuides				= bSimulationEnable || bDeformationEnable || bGlobalInterpolationEnable || bSimulationCacheEnable;

		const EHairResourceLoadingType LoadingType = GetHairResourceLoadingType(GeometryType, int32(LODIndex));
		if (LoadingType != EHairResourceLoadingType::Async || GeometryType == EHairGeometryType::NoneGeometry)
		{
			return;
		}

		// Lazy allocation of resources
		// Note: Allocation will only be done if the resources is not initialized yet. Guides deformed position are also initialized from the Rest position at creation time.
		if (Instance->Guides.Data && bLODNeedsGuides)
		{
			if (Instance->Guides.RestRootResource)			{ Instance->Guides.RestRootResource->StreamInData(MeshLODIndex);}
			if (Instance->Guides.RestResource)				{ Instance->Guides.RestResource->StreamInData(); }
		}

		if (GeometryType == EHairGeometryType::Meshes)
		{
			FHairGroupInstance::FMeshes::FLOD& InstanceLOD = Instance->Meshes.LODs[IntLODIndex];
		}
		else if (GeometryType == EHairGeometryType::Cards)
		{
			FHairGroupInstance::FCards::FLOD& InstanceLOD = Instance->Cards.LODs[IntLODIndex];

			if (InstanceLOD.InterpolationResource)			{ InstanceLOD.InterpolationResource->StreamInData(); }
			if (InstanceLOD.DeformedResource)				{ InstanceLOD.DeformedResource->StreamInData(); }
			if (InstanceLOD.Guides.RestRootResource)		{ InstanceLOD.Guides.RestRootResource->StreamInData(MeshLODIndex); }
			if (InstanceLOD.Guides.RestResource)			{ InstanceLOD.Guides.RestResource->StreamInData(); }
			if (InstanceLOD.Guides.InterpolationResource)	{ InstanceLOD.Guides.InterpolationResource->StreamInData(); }
		}
		else if (GeometryType == EHairGeometryType::Strands)
		{
			if (Instance->Strands.RestRootResource)			{ Instance->Strands.RestRootResource->StreamInData(MeshLODIndex); }
			if (Instance->Strands.RestResource)				{ Instance->Strands.RestResource->StreamInData(); }
			if (Instance->Strands.ClusterResource)			{ Instance->Strands.ClusterResource->StreamInData(); }
			if (Instance->Strands.InterpolationResource)	{ Instance->Strands.InterpolationResource->StreamInData(); }
		}
	}
}

static FVector2f ComputeProjectedScreenPos(const FVector& InWorldPos, const FSceneView& View)
{
	// Compute the MinP/MaxP in pixel coord, relative to View.ViewRect.Min
	const FMatrix& WorldToView = View.ViewMatrices.GetViewMatrix();
	const FMatrix& ViewToProj = View.ViewMatrices.GetProjectionMatrix();
	const float NearClippingDistance = View.NearClippingDistance + SMALL_NUMBER;
	const FIntRect ViewRect = View.UnconstrainedViewRect;

	// Clamp position on the near plane to get valid rect even if bounds' points are behind the camera
	FPlane P_View = WorldToView.TransformFVector4(FVector4(InWorldPos, 1.f));
	if (P_View.Z <= NearClippingDistance)
	{
		P_View.Z = NearClippingDistance;
	}

	// Project from view to projective space
	FVector2D MinP(FLT_MAX, FLT_MAX);
	FVector2D MaxP(-FLT_MAX, -FLT_MAX);
	FVector2D ScreenPos;
	const bool bIsValid = FSceneView::ProjectWorldToScreen(P_View, ViewRect, ViewToProj, ScreenPos);

	// Clamp to pixel border
	ScreenPos = FIntPoint(FMath::FloorToInt(ScreenPos.X), FMath::FloorToInt(ScreenPos.Y));

	// Clamp to screen rect
	ScreenPos.X = FMath::Clamp(ScreenPos.X, ViewRect.Min.X, ViewRect.Max.X);
	ScreenPos.Y = FMath::Clamp(ScreenPos.Y, ViewRect.Min.Y, ViewRect.Max.Y);

	return FVector2f(ScreenPos.X, ScreenPos.Y);
}

static float ComputeActiveCurveCoverageScale(uint32 InAvailableCurveCount, uint32 InRestCurveCount)
{
	// Compensate lost in curve by a coverage scale increase
	const float CurveRatio = InAvailableCurveCount / float(InRestCurveCount);
	return 1.f/FMath::Max(CurveRatio, 0.01f);
}

static uint32 ComputeActiveCurveCount(float InScreenSize, uint32 InCurveCount, uint32 InClusterCount)
{
	const float Power = FMath::Max(0.1f, GHairStrands_AutoLOD_Scale);
	const float ScreenSizeBias = FMath::Clamp(GHairStrands_AutoLOD_Bias, 0.f, 1.f);
	uint32 OutCurveCount = InCurveCount * FMath::Pow(FMath::Clamp(InScreenSize + ScreenSizeBias, 0.f, 1.0f), Power);
	// Ensure there is at least 1 curve per cluster
	OutCurveCount = FMath::Max(InClusterCount, OutCurveCount);
	return FMath::Clamp(OutCurveCount, 1, InCurveCount);
}

static void RunHairLODSelection(
	FRDGBuilder& GraphBuilder, 
	FSceneInterface* Scene,
	const FHairStrandsInstances& Instances, 
	const TArray<const FSceneView*>& Views, 
	FGlobalShaderMap* ShaderMap)
{
	EShaderPlatform ShaderPlatform = EShaderPlatform::SP_NumPlatforms;
	if (Views.Num() > 0)
	{
		ShaderPlatform = Views[0]->GetShaderPlatform();
	}

	// Detect if one of the current view has path tracing enabled for allocating raytracing resources
#if RHI_RAYTRACING
	bool bHasPathTracingView = false;
	for (const FSceneView* View : Views)
	{
		if (View->Family->EngineShowFlags.PathTracing)
		{
			bHasPathTracingView = true;
			break;
		}
	}
#endif

	for (FHairStrandsInstance* AbstractInstance : Instances)
	{
		FHairGroupInstance* Instance = static_cast<FHairGroupInstance*>(AbstractInstance);

		check(Instance);
		check(Instance->HairGroupPublicData);
		const FCachedGeometry CachedGeometry = GetCacheGeometryForHair(GraphBuilder, Scene, Instance, ShaderMap, false);
		const int32 MeshLODIndex = GetCacheGeometryLODIndex(CachedGeometry);

		// Perform LOD selection based on all the views	
		// CPU LOD selection. 
		// * When enable the CPU LOD selection allow to change the geometry representation. 
		// * GPU LOD selecion allow fine grain LODing, but does not support representation changes (strands, cards, meshes)
		// 
		// Use the forced LOD index if set, otherwise compute the LOD based on the maximal screensize accross all views
		// Compute the view where the screen size is maximale
		const float PrevLODIndex = Instance->HairGroupPublicData->GetLODIndex();
		const float PrevMeshLODIndex = Instance->HairGroupPublicData->GetMeshLODIndex();

		// 0. Initial LOD index picking
		// Insure that MinLOD is necessary taken into account if a force LOD is request (i.e., LODIndex>=0). If a Force LOD 
		// is not requested (i.e., LODIndex<0), the MinLOD is applied after ViewLODIndex has been determined in the codeblock below
		const int32 LODCount = Instance->HairGroupPublicData->GetLODVisibilities().Num();
		const float MinLOD = FMath::Max(0, GHairStrandsMinLOD);
		
		// If continuous LOD is enabled, we bypass all other type of geometric representation, and only use LOD0
		float LODIndex = IsHairVisibilityComputeRasterContinuousLODEnabled() ? 0.0 : (Instance->Debug.LODForcedIndex >= 0 ? FMath::Max(Instance->Debug.LODForcedIndex, MinLOD) : -1.0f);
		{
			FVector2f MaxContinuousLODScreenPos = FVector2f(0.f, 0.f);
			float LODViewIndex = -1;
			float MaxScreenSize_RestBound = 0.f;
			float MaxScreenSize_Bound = 0.f;
			const FSphere SphereBound = Instance->GetBounds().GetSphere();
			for (const FSceneView* View : Views)
			{
				const FVector3d BoundScale = Instance->LocalToWorld.GetScale3D();
				const FVector3f BoundExtent = Instance->Strands.Data ? FVector3f(Instance->Strands.Data->Header.BoundingBox.GetExtent()) : FVector3f(0,0,0);
				const float BoundRadius = FMath::Max3(BoundExtent.X, BoundExtent.Y, BoundExtent.Z) * FMath::Max3(BoundScale.X, BoundScale.Y, BoundScale.Z);

				const float ScreenSize_RestBound = FMath::Clamp(ComputeBoundsScreenSize(FVector4(SphereBound.Center, 1), BoundRadius, *View), 0.f, 1.0f);
				const float ScreenSize_Bound = FMath::Clamp(ComputeBoundsScreenSize(FVector4(SphereBound.Center, 1), SphereBound.W, *View), 0.f, 1.0f);

				const float CurrLODViewIndex = FMath::Max(MinLOD, GetHairInstanceLODIndex(Instance->HairGroupPublicData->GetLODScreenSizes(), ScreenSize_Bound, Instance->Strands.Modifier.LODBias));

				MaxScreenSize_Bound = FMath::Max(MaxScreenSize_Bound, ScreenSize_Bound);
				MaxScreenSize_RestBound = FMath::Max(MaxScreenSize_RestBound, ScreenSize_RestBound);
				MaxContinuousLODScreenPos = ComputeProjectedScreenPos(SphereBound.Center, *View);

				// Select highest LOD across all views
				LODViewIndex = LODViewIndex < 0 ? CurrLODViewIndex : FMath::Min(LODViewIndex, CurrLODViewIndex);
			}

			if (LODIndex < 0)
			{
				LODIndex = LODViewIndex;
			}
			LODIndex = FMath::Clamp(LODIndex, 0.f, float(LODCount - 1));

			// Feedback game thread with LOD selection 
			Instance->Debug.LODPredictedIndex = LODViewIndex;
			Instance->HairGroupPublicData->DebugScreenSize = MaxScreenSize_Bound;
			Instance->HairGroupPublicData->ContinuousLODPointCount = Instance->HairGroupPublicData->RestPointCount;
			Instance->HairGroupPublicData->ContinuousLODCurveCount = Instance->HairGroupPublicData->RestCurveCount;
			Instance->HairGroupPublicData->ContinuousLODScreenSize = MaxScreenSize_RestBound;
			Instance->HairGroupPublicData->ContinuousLODScreenPos = MaxContinuousLODScreenPos;
			Instance->HairGroupPublicData->ContinuousLODBounds = SphereBound;
			Instance->HairGroupPublicData->ContinuousLODCoverageScale = 1.f;

			if (Instance->Strands.ClusterResource)
			{
				uint32 EffectiveCurveCount = 0;
				if (Instance->HairGroupPublicData->bAutoLOD || IsHairStrandsForceAutoLODEnabled())
				{
					EffectiveCurveCount = ComputeActiveCurveCount(Instance->HairGroupPublicData->ContinuousLODScreenSize, Instance->HairGroupPublicData->RestCurveCount, Instance->HairGroupPublicData->ClusterCount);
					LODIndex = 0;
				}
				else
				{
					EffectiveCurveCount = Instance->Strands.ClusterResource->BulkData.GetCurveCount(LODIndex);
				}
				check(EffectiveCurveCount <= uint32(Instance->Strands.Data->Header.CurveToPointCount.Num()));

				Instance->HairGroupPublicData->ContinuousLODCurveCount = EffectiveCurveCount;
				Instance->HairGroupPublicData->ContinuousLODPointCount = EffectiveCurveCount > 0 ? Instance->Strands.Data->Header.CurveToPointCount[EffectiveCurveCount - 1] : 0;
				Instance->HairGroupPublicData->ContinuousLODCoverageScale = ComputeActiveCurveCoverageScale(EffectiveCurveCount, Instance->HairGroupPublicData->RestCurveCount);
			}
		}

		// Function for selecting, loading, & initializing LOD resources
		auto SelectValidLOD = [Instance, ShaderPlatform, ShaderMap, PrevLODIndex, LODCount, MeshLODIndex, PrevMeshLODIndex
		#if RHI_RAYTRACING
		, bHasPathTracingView
		#endif // RHI_RAYTRACING
		] (FRDGBuilder& GraphBuilder, float LODIndex) -> bool
		{
			const int32 IntLODIndex = FMath::Clamp(FMath::FloorToInt(LODIndex), 0, LODCount - 1);
			const TArray<EHairGeometryType>& LODGeometryTypes = Instance->HairGroupPublicData->GetLODGeometryTypes();
			const TArray<bool>& LODVisibilities = Instance->HairGroupPublicData->GetLODVisibilities();
			const bool bIsVisible = LODVisibilities[IntLODIndex];
			const bool bForceCards = GHairStrands_UseCards > 0 || Instance->bForceCards; // todo
			EHairGeometryType GeometryType = ConvertLODGeometryType(LODGeometryTypes[IntLODIndex], bForceCards, ShaderPlatform);

			// Skip cluster culling if strands have a single LOD (i.e.: LOD0), and the instance is static (i.e., no skinning, no simulation, ...)
			bool bCullingEnable = false;
			if (GeometryType == EHairGeometryType::Meshes)
			{
				if (!Instance->Meshes.IsValid(IntLODIndex))
				{
					GeometryType = EHairGeometryType::NoneGeometry;
				}
			}
			else if (GeometryType == EHairGeometryType::Cards)
			{
				if (!Instance->Cards.IsValid(IntLODIndex))
				{
					GeometryType = EHairGeometryType::NoneGeometry;
				}
			}
			else if (GeometryType == EHairGeometryType::Strands)
			{
				if (!Instance->Strands.IsValid())
				{
					GeometryType = EHairGeometryType::NoneGeometry;
				}
				else
				{
					const bool bNeedLODing		= LODIndex > 0;
					const bool bNeedDeformation = Instance->Strands.DeformedResource != nullptr;
					bCullingEnable = (bNeedLODing || bNeedDeformation);
				}
			}

			if (!bIsVisible)
			{
				GeometryType = EHairGeometryType::NoneGeometry;
			}

			// Transform ContinuousLOD CurveCount/PointCount into streaming request
			uint32 RequestedCurveCount = Instance->HairGroupPublicData->RestCurveCount;
			uint32 RequestedPointCount = Instance->HairGroupPublicData->RestPointCount;
			const bool bStreamingEnabled = GHairStrands_Streaming > 0;
			if (GeometryType == EHairGeometryType::Strands && bStreamingEnabled && Instance->bSupportStreaming)
			{
				// Round Curve/Point request to curve 'page'
				RequestedCurveCount = GetRoundedCurveCount(Instance->HairGroupPublicData->ContinuousLODCurveCount, Instance->HairGroupPublicData->RestCurveCount);
				RequestedPointCount = RequestedCurveCount > 0 ? Instance->Strands.Data->Header.CurveToPointCount[RequestedCurveCount - 1] : 0;
			}

			const bool bSimulationEnable			= Instance->HairGroupPublicData->IsSimulationEnable(IntLODIndex);
			const bool bDeformationEnable			= Instance->HairGroupPublicData->bIsDeformationEnable;
			const bool bGlobalInterpolationEnable	= Instance->HairGroupPublicData->IsGlobalInterpolationEnable(IntLODIndex);
			const bool bSimulationCacheEnable		= Instance->HairGroupPublicData->bIsSimulationCacheEnable;
			const bool bLODNeedsGuides				= bSimulationEnable || bDeformationEnable || bGlobalInterpolationEnable || bSimulationCacheEnable;

			const EHairResourceLoadingType LoadingType = GetHairResourceLoadingType(GeometryType, IntLODIndex);
			EHairResourceStatus ResourceStatus;
			ResourceStatus.Status 				= EHairResourceStatus::EStatus::None;
			ResourceStatus.AvailableCurveCount 	= Instance->HairGroupPublicData->RestCurveCount;

			// Lazy allocation of resources
			// Note: Allocation will only be done if the resources is not initialized yet. Guides deformed position are also initialized from the Rest position at creation time.
			if (Instance->Guides.Data && bLODNeedsGuides)
			{
				if (Instance->Guides.RestRootResource)			{ Instance->Guides.RestRootResource->Allocate(GraphBuilder, LoadingType, ResourceStatus, MeshLODIndex); }
				if (Instance->Guides.RestResource)				{ Instance->Guides.RestResource->Allocate(GraphBuilder, LoadingType, ResourceStatus); }
				if (Instance->Guides.DeformedRootResource)		{ Instance->Guides.DeformedRootResource->Allocate(GraphBuilder, LoadingType, ResourceStatus, MeshLODIndex); }
				if (Instance->Guides.DeformedResource)			
				{ 
					// Ensure the rest resources are correctly loaded prior to initialized and copy the rest position into the deformed positions
					if (Instance->Guides.RestResource->bIsInitialized)
					{
						const bool bNeedCopy = !Instance->Guides.DeformedResource->bIsInitialized; 
						Instance->Guides.DeformedResource->Allocate(GraphBuilder, EHairResourceLoadingType::Sync); 
						if (bNeedCopy) 
						{ 
							AddCopyHairStrandsPositionPass(GraphBuilder, ShaderMap, *Instance->Guides.RestResource, *Instance->Guides.DeformedResource); 
						}
					}
				}
			}

			if (GeometryType == EHairGeometryType::Meshes)
			{
				FHairGroupInstance::FMeshes::FLOD& InstanceLOD = Instance->Meshes.LODs[IntLODIndex];

				if (InstanceLOD.DeformedResource)				{ InstanceLOD.DeformedResource->Allocate(GraphBuilder, LoadingType, ResourceStatus); }
				#if RHI_RAYTRACING
				if (InstanceLOD.RaytracingResource)				{ InstanceLOD.RaytracingResource->Allocate(GraphBuilder, LoadingType, ResourceStatus);}
				#endif

				InstanceLOD.InitVertexFactory();
			}
			else if (GeometryType == EHairGeometryType::Cards)
			{
				FHairGroupInstance::FCards::FLOD& InstanceLOD = Instance->Cards.LODs[IntLODIndex];

				if (InstanceLOD.InterpolationResource)			{ InstanceLOD.InterpolationResource->Allocate(GraphBuilder, LoadingType, ResourceStatus); }
				if (InstanceLOD.DeformedResource)				{ InstanceLOD.DeformedResource->Allocate(GraphBuilder, LoadingType, ResourceStatus); }
				if (InstanceLOD.Guides.RestRootResource)		{ InstanceLOD.Guides.RestRootResource->Allocate(GraphBuilder, LoadingType, ResourceStatus, MeshLODIndex); }
				if (InstanceLOD.Guides.RestResource)			{ InstanceLOD.Guides.RestResource->Allocate(GraphBuilder, LoadingType, ResourceStatus); }
				if (InstanceLOD.Guides.DeformedRootResource)	{ InstanceLOD.Guides.DeformedRootResource->Allocate(GraphBuilder, LoadingType, ResourceStatus, MeshLODIndex); }
				if (InstanceLOD.Guides.DeformedResource)		{ InstanceLOD.Guides.DeformedResource->Allocate(GraphBuilder, LoadingType, ResourceStatus); }
				if (InstanceLOD.Guides.InterpolationResource)	{ InstanceLOD.Guides.InterpolationResource->Allocate(GraphBuilder, LoadingType, ResourceStatus); }
				#if RHI_RAYTRACING
				if (InstanceLOD.RaytracingResource)				{ InstanceLOD.RaytracingResource->Allocate(GraphBuilder, LoadingType, ResourceStatus);}
				#endif
				InstanceLOD.InitVertexFactory();
			}
			else if (GeometryType == EHairGeometryType::Strands)
			{
				check(Instance->HairGroupPublicData);

				if (Instance->Strands.RestRootResource)			{ Instance->Strands.RestRootResource->Allocate(GraphBuilder, EHairResourceLoadingType::Async, ResourceStatus, RequestedCurveCount, RequestedPointCount, MeshLODIndex); }
				if (Instance->Strands.RestResource)				{ Instance->Strands.RestResource->Allocate(GraphBuilder, EHairResourceLoadingType::Async, ResourceStatus, RequestedCurveCount, RequestedPointCount); }
				if (Instance->Strands.ClusterResource)			{ Instance->Strands.ClusterResource->Allocate(GraphBuilder, EHairResourceLoadingType::Async, ResourceStatus, RequestedCurveCount, RequestedPointCount); }
				if (Instance->Strands.InterpolationResource)	{ Instance->Strands.InterpolationResource->Allocate(GraphBuilder, EHairResourceLoadingType::Async, ResourceStatus, RequestedCurveCount, RequestedPointCount); }
				if (Instance->Strands.CullingResource)			{ Instance->Strands.CullingResource->Allocate(GraphBuilder, EHairResourceLoadingType::Async, ResourceStatus, RequestedCurveCount, RequestedPointCount); }

				if (Instance->Strands.DeformedRootResource)		{ Instance->Strands.DeformedRootResource->Allocate(GraphBuilder, EHairResourceLoadingType::Async, ResourceStatus, RequestedCurveCount, RequestedPointCount, MeshLODIndex); }
				if (Instance->Strands.DeformedResource)			{ Instance->Strands.DeformedResource->Allocate(GraphBuilder, EHairResourceLoadingType::Async, ResourceStatus, RequestedCurveCount, RequestedPointCount); }
				#if RHI_RAYTRACING
				if (bHasPathTracingView)						{ AllocateRaytracingResources(Instance); }
				if (Instance->Strands.RenRaytracingResource)	{ Instance->Strands.RenRaytracingResource->Allocate(GraphBuilder, EHairResourceLoadingType::Async, ResourceStatus, RequestedCurveCount, RequestedPointCount); }
				#endif
				Instance->Strands.VertexFactory->InitResources(GraphBuilder.RHICmdList);

				// Early initialization, so that when filtering MeshBatch block in SceneVisiblity, we can use this value to know 
				// if the hair instance is visible or not (i.e., HairLengthScale > 0)
				Instance->HairGroupPublicData->VFInput.Strands.Common.LengthScale = Instance->Strands.Modifier.HairLengthScale;
			}

			// Only switch LOD if the data are ready to be used
			const bool bIsLODDataReady = !ResourceStatus.HasStatus(EHairResourceStatus::EStatus::Loading) || (bStreamingEnabled && ResourceStatus.AvailableCurveCount > 0);
			if (bIsLODDataReady)
			{
				const uint32 IntPrevLODIndex = FMath::FloorToInt(PrevLODIndex); 
				const EHairBindingType CurrBindingType = Instance->HairGroupPublicData->GetBindingType(IntLODIndex);
				const EHairBindingType PrevBindingType = Instance->HairGroupPublicData->GetBindingType(IntPrevLODIndex);

				Instance->HairGroupPublicData->SetLODVisibility(bIsVisible);
				Instance->HairGroupPublicData->SetLODIndex(LODIndex);
				Instance->HairGroupPublicData->SetLODBias(0);
				Instance->HairGroupPublicData->SetMeshLODIndex(MeshLODIndex);
				Instance->HairGroupPublicData->VFInput.GeometryType = GeometryType;
				Instance->HairGroupPublicData->VFInput.BindingType = CurrBindingType;
				Instance->HairGroupPublicData->VFInput.bHasLODSwitch = IntPrevLODIndex != IntLODIndex;
				Instance->HairGroupPublicData->VFInput.bHasLODSwitchBindingType = CurrBindingType != PrevBindingType;
				Instance->GeometryType = GeometryType;
				Instance->BindingType = CurrBindingType;
				Instance->Guides.bIsSimulationEnable = Instance->HairGroupPublicData->IsSimulationEnable(IntLODIndex);
				Instance->Guides.bHasGlobalInterpolation = Instance->HairGroupPublicData->IsGlobalInterpolationEnable(IntLODIndex);
				Instance->Guides.bIsDeformationEnable = Instance->HairGroupPublicData->bIsDeformationEnable;
				Instance->Guides.bIsSimulationCacheEnable = Instance->HairGroupPublicData->bIsSimulationCacheEnable;
				Instance->Strands.bCullingEnable = bCullingEnable;
			}

			// Ensure that if the binding type is set to Skinning (or has RBF enabled), the skel. mesh is valid and support skin cache.
			if ((Instance->Guides.bHasGlobalInterpolation || Instance->BindingType == EHairBindingType::Skinning) && MeshLODIndex < 0)
			{
				return false;
			}

			// If requested LOD's resources are not ready yet, and the previous LOD was invalid or if the MeshLODIndex has changed 
			// (which would required extra data loading) change the geometry to be invalid, so that it don't get processed this frame.
			const bool bIsLODValid = PrevLODIndex >= 0;
			const bool bHasMeshLODChanged = PrevMeshLODIndex != MeshLODIndex && (Instance->Guides.bHasGlobalInterpolation || Instance->BindingType == EHairBindingType::Skinning);
			if (!bIsLODDataReady && (!bIsLODValid || bHasMeshLODChanged))
			{
				return false;
			}

			// Adapt the number curve/point based on available curves/points
			if (GeometryType == EHairGeometryType::Strands && bStreamingEnabled)
			{
				if (!bIsLODDataReady)
				{
					return false;
				}
				check(bIsLODDataReady);
				check(Instance->Strands.RestResource);

				// Adapt CurveCount/PointCount/CoverageScale based on what is actually available
				const uint32 EffectiveCurveCount = FMath::Min(ResourceStatus.AvailableCurveCount, Instance->HairGroupPublicData->ContinuousLODCurveCount);
				Instance->HairGroupPublicData->ContinuousLODCurveCount = EffectiveCurveCount;
				Instance->HairGroupPublicData->ContinuousLODPointCount = EffectiveCurveCount > 0 ? Instance->Strands.Data->Header.CurveToPointCount[EffectiveCurveCount - 1] : 0;
				Instance->HairGroupPublicData->ContinuousLODCoverageScale = ComputeActiveCurveCoverageScale(EffectiveCurveCount, Instance->HairGroupPublicData->RestCurveCount);
				check(Instance->HairGroupPublicData->ContinuousLODPointCount <= Instance->HairGroupPublicData->RestPointCount);
			}
			return true;
		};

		// 1. Try to find an available LOD
		bool bFoundValidLOD = SelectValidLOD(GraphBuilder, LODIndex);

		// 2. If no valid LOD are found, try to find a coarser LOD which is:
		// * loaded 
		// * does not require skinning binding 
		// * does not require global interpolation (as global interpolation requires skel. mesh data)
		// * does not require simulation (as simulation requires LOD to be selected on the game thread i.e., Predicted/Forced, and so we can override this LOD here)
		if (!bFoundValidLOD)
		{
			for (int32 FallbackLODIndex = FMath::Clamp(FMath::FloorToInt(LODIndex + 1), 0, LODCount - 1); FallbackLODIndex < LODCount; ++FallbackLODIndex)
			{
				if (Instance->HairGroupPublicData->GetBindingType(FallbackLODIndex) == EHairBindingType::Rigid &&
					!Instance->HairGroupPublicData->IsSimulationEnable(FallbackLODIndex) &&
					!Instance->HairGroupPublicData->IsGlobalInterpolationEnable(FallbackLODIndex))
				{
					bFoundValidLOD = SelectValidLOD(GraphBuilder, FallbackLODIndex);
					break;
				}
			}
		}

		// 3. If no LOD are valid, then mark the instance as invalid for this frame
		if (!bFoundValidLOD)
		{
			Instance->GeometryType = EHairGeometryType::NoneGeometry;
			Instance->BindingType = EHairBindingType::NoneBinding;
		}

		// Update the local-to-world transform based on the binding type 
		if (GetHairSwapBufferType() != EHairBufferSwapType::Tick)
		{
			Instance->Debug.SkinningPreviousLocalToWorld = Instance->Debug.SkinningCurrentLocalToWorld;
			Instance->Debug.SkinningCurrentLocalToWorld = CachedGeometry.LocalToWorld;
		}
		Instance->LocalToWorld = Instance->GetCurrentLocalToWorld();
	}
}


///////////////////////////////////////////////////////////////////////////////////////////////////
bool HasHairStrandsFolliculeMaskQueries();
void RunHairStrandsFolliculeMaskQueries(FRDGBuilder& GraphBuilder, FGlobalShaderMap* ShaderMap);

#if WITH_EDITOR
bool HasHairStrandsTexturesQueries();
void RunHairStrandsTexturesQueries(FRDGBuilder& GraphBuilder, FGlobalShaderMap* ShaderMap, const struct FShaderPrintData* ShaderPrintData);

bool HasHairStrandsPositionQueries();
void RunHairStrandsPositionQueries(FRDGBuilder& GraphBuilder, FGlobalShaderMap* ShaderMap, const struct FShaderPrintData* DebugShaderData);
#endif

static void RunHairStrandsProcess(FRDGBuilder& GraphBuilder, FGlobalShaderMap* ShaderMap, const struct FShaderPrintData* ShaderPrintData)
{
#if WITH_EDITOR
	if (HasHairStrandsTexturesQueries())
	{
		RunHairStrandsTexturesQueries(GraphBuilder, ShaderMap, ShaderPrintData);
	}

	if (HasHairStrandsPositionQueries())
	{
		RunHairStrandsPositionQueries(GraphBuilder, ShaderMap, ShaderPrintData);
	}
#endif

	if (HasHairStrandsFolliculeMaskQueries())
	{
		RunHairStrandsFolliculeMaskQueries(GraphBuilder, ShaderMap);
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void RunHairStrandsDebug(
	FRDGBuilder& GraphBuilder,
	FGlobalShaderMap* ShaderMap,
	FSceneInterface* Scene,
	const FSceneView& View,
	const FHairStrandsInstances& Instances,
	const TArray<EHairInstanceVisibilityType>& InstancesVisibilityType,
	FHairTransientResources& TransientResources,
	const FShaderPrintData* ShaderPrintData,
	FRDGTextureRef SceneColor,
	FRDGTextureRef SceneDepth,
	FIntRect Viewport,
	const TUniformBufferRef<FViewUniformShaderParameters>& ViewUniformBuffer);

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// HairStrands Bookmark API

void ProcessHairStrandsBookmark(
	FRDGBuilder* GraphBuilder,
	EHairStrandsBookmark Bookmark,
	FHairStrandsBookmarkParameters& Parameters)
{
	check(Parameters.Instances != nullptr);

	FHairStrandsInstances* Instances = Parameters.Instances;

	// Cards interpolation for ShadowView only needs to run when FrustumCulling is enabled
	if (Bookmark == EHairStrandsBookmark::ProcessCardsAndMeshesInterpolation_ShadowView && !IsInstanceFrustumCullingEnable())
	{
		return;
	}

	if (IsInstanceFrustumCullingEnable())
	{
		Instances = nullptr;
		if (Bookmark == EHairStrandsBookmark::ProcessCardsAndMeshesInterpolation_PrimaryView)
		{
			Instances = &Parameters.VisibleCardsOrMeshes_Primary;
		}
		else if (Bookmark == EHairStrandsBookmark::ProcessCardsAndMeshesInterpolation_ShadowView)
		{
			Instances = &Parameters.VisibleCardsOrMeshes_Shadow;
		}
		else if (Bookmark == EHairStrandsBookmark::ProcessStrandsInterpolation)
		{
			Instances = &Parameters.VisibleStrands;
		}
		else
		{
			Instances = Parameters.Instances;
		}
	}

	if (Bookmark == EHairStrandsBookmark::ProcessTasks)
	{
		const bool bHasHairStardsnProcess =
		#if WITH_EDITOR
			HasHairStrandsTexturesQueries() ||
			HasHairStrandsPositionQueries() ||
		#endif
			HasHairStrandsFolliculeMaskQueries();

		if (bHasHairStardsnProcess)
		{
			check(GraphBuilder);
			RunHairStrandsProcess(*GraphBuilder, Parameters.ShaderMap, Parameters.ShaderPrintData);
		}
	}
	else if (Bookmark == EHairStrandsBookmark::ProcessLODSelection)
	{
		if (GetHairSwapBufferType() == EHairBufferSwapType::BeginOfFrame)
		{
			RunHairBufferSwap(
				*Parameters.Instances,
				Parameters.AllViews);
		}
		check(GraphBuilder);
		RunHairLODSelection(
			*GraphBuilder,
			Parameters.Scene,
			*Parameters.Instances,
			Parameters.AllViews,
			Parameters.ShaderMap);
	}
	else if (Bookmark == EHairStrandsBookmark::ProcessEndOfFrame)
	{
		if (GetHairSwapBufferType() == EHairBufferSwapType::EndOfFrame)
		{
			RunHairBufferSwap(
				*Parameters.Instances,
				Parameters.AllViews);
		}
	}
	else if (Bookmark == EHairStrandsBookmark::ProcessGuideInterpolation)
	{
		check(GraphBuilder);
		RunHairStrandsInterpolation_Guide(
			*GraphBuilder,
			Parameters.Scene,
			Parameters.View,
			Parameters.ViewUniqueID,
			*Instances,
			Parameters.ShaderPrintData,
			Parameters.ShaderMap);
	}
	else if (Bookmark == EHairStrandsBookmark::ProcessCardsAndMeshesInterpolation_PrimaryView || Bookmark == EHairStrandsBookmark::ProcessCardsAndMeshesInterpolation_ShadowView)
	{
		check(GraphBuilder);
		RunHairStrandsInterpolation_Cards(
			*GraphBuilder,
			Parameters.Scene,
			Parameters.AllViews,
			Parameters.View,
			Parameters.ViewUniqueID,
			*Instances,
			Parameters.ShaderPrintData,
			Parameters.ShaderMap);
	}
	else if (Bookmark == EHairStrandsBookmark::ProcessStrandsInterpolation)
	{
		check(GraphBuilder);
		check(Parameters.TransientResources);
		RunHairStrandsInterpolation_Strands(
			*GraphBuilder,
			Parameters.Scene,
			Parameters.AllViews,
			Parameters.View,
			Parameters.ViewUniqueID,
			Parameters.Instances->Num(),
			*Instances,
			Parameters.ShaderPrintData,
			*Parameters.TransientResources,
			Parameters.ShaderMap);
	}
	else if (Bookmark == EHairStrandsBookmark::ProcessDebug)
	{
		// Merge all visible instances
		FHairStrandsInstances DebugInstances;
		if (IsInstanceFrustumCullingEnable())
		{
			DebugInstances.Append(Parameters.VisibleStrands);
			DebugInstances.Append(Parameters.VisibleCardsOrMeshes_Primary);
			DebugInstances.Append(Parameters.VisibleCardsOrMeshes_Shadow);
			DebugInstances.Sort([](const FHairStrandsInstance& A, const FHairStrandsInstance& B)
			{
				return A.RegisteredIndex < B.RegisteredIndex;
			});
			Instances = &DebugInstances;
		}

		check(GraphBuilder);
		check(Parameters.TransientResources);
		RunHairStrandsDebug(
			*GraphBuilder,
			Parameters.ShaderMap,
			Parameters.Scene,
			*Parameters.View,
			*Instances,
			Parameters.InstancesVisibilityType,
			*Parameters.TransientResources,
			Parameters.ShaderPrintData,
			Parameters.SceneColorTexture,
			Parameters.SceneDepthTexture,
			Parameters.View->UnscaledViewRect,
			Parameters.View->ViewUniformBuffer);
	}
}
