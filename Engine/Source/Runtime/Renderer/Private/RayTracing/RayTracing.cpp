// Copyright Epic Games, Inc. All Rights Reserved.

#include "RayTracing.h"

#if RHI_RAYTRACING

#include "RayTracingDynamicGeometryCollection.h"
#include "RayTracingInstanceMask.h"
#include "RayTracingInstanceCulling.h"
#include "RayTracingMaterialHitShaders.h"
#include "RayTracingScene.h"
#include "Nanite/NaniteRayTracing.h"
#include "Rendering/NaniteCoarseMeshStreamingManager.h"
#include "Rendering/RayTracingGeometryManager.h"
#include "ScenePrivate.h"
#include "Materials/MaterialRenderProxy.h"
#include "Lumen/LumenHardwareRayTracingCommon.h"
#include "RayTracingShadows.h"
#include "Experimental/Containers/SherwoodHashTable.h"
#include "RHIShaderBindingLayout.h"
#include "Async/ParallelFor.h"
#include <type_traits>

static int32 GRayTracingSceneCaptures = -1;
static FAutoConsoleVariableRef CVarRayTracingSceneCaptures(
	TEXT("r.RayTracing.SceneCaptures"),
	GRayTracingSceneCaptures,
	TEXT("Enable ray tracing in scene captures.\n")
	TEXT(" -1: Use scene capture settings (default) \n")
	TEXT(" 0: off \n")
	TEXT(" 1: on"),
	ECVF_RenderThreadSafe);

static int32 GRayTracingParallelMeshBatchSetup = 1;
static FAutoConsoleVariableRef CRayTracingParallelMeshBatchSetup(
	TEXT("r.RayTracing.ParallelMeshBatchSetup"),
	GRayTracingParallelMeshBatchSetup,
	TEXT("Whether to setup ray tracing materials via parallel jobs."),
	ECVF_RenderThreadSafe);

static int32 GRayTracingParallelMeshBatchSize = 1024;
static FAutoConsoleVariableRef CRayTracingParallelMeshBatchSize(
	TEXT("r.RayTracing.ParallelMeshBatchSize"),
	GRayTracingParallelMeshBatchSize,
	TEXT("Batch size for ray tracing materials parallel jobs."),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> CVarRayTracingDynamicGeometryLastRenderTimeUpdateDistance(
	TEXT("r.RayTracing.DynamicGeometryLastRenderTimeUpdateDistance"),
	5000.0f,
	TEXT("Dynamic geometries within this distance will have their LastRenderTime updated, so that visibility based ticking (like skeletal mesh) can work when the component is not directly visible in the view (but reflected)."));

static TAutoConsoleVariable<int32> CVarRayTracingAutoInstance(
	TEXT("r.RayTracing.AutoInstance"),
	1,
	TEXT("Whether to auto instance static meshes\n"),
	ECVF_RenderThreadSafe
);

static int32 GRayTracingExcludeTranslucent = 0;
static FAutoConsoleVariableRef CRayTracingExcludeTranslucent(
	TEXT("r.RayTracing.ExcludeTranslucent"),
	GRayTracingExcludeTranslucent,
	TEXT("A toggle that modifies the inclusion of translucent objects in the ray tracing scene.\n")
	TEXT(" 0: Translucent objects included in the ray tracing scene (default)\n")
	TEXT(" 1: Translucent objects excluded from the ray tracing scene"),
	ECVF_RenderThreadSafe);

static int32 GRayTracingExcludeSky = 1;
static FAutoConsoleVariableRef CRayTracingExcludeSky(
	TEXT("r.RayTracing.ExcludeSky"),
	GRayTracingExcludeSky,
	TEXT("A toggle that controls inclusion of sky geometry in the ray tracing scene (excluding sky can make ray tracing faster). This setting is ignored for the Path Tracer.\n")
	TEXT(" 0: Sky objects included in the ray tracing scene\n")
	TEXT(" 1: Sky objects excluded from the ray tracing scene (default)"),
	ECVF_RenderThreadSafe);

static int32 GRayTracingExcludeDecals = 0;
static FAutoConsoleVariableRef CRayTracingExcludeDecals(
	TEXT("r.RayTracing.ExcludeDecals"),
	GRayTracingExcludeDecals,
	TEXT("A toggle that modifies the inclusion of decals in the ray tracing BVH.\n")
	TEXT(" 0: Decals included in the ray tracing BVH (default)\n")
	TEXT(" 1: Decals excluded from the ray tracing BVH"),
	ECVF_RenderThreadSafe);

static int32 GRayTracingDebugDisableTriangleCull = 0;
static FAutoConsoleVariableRef CVarRayTracingDebugDisableTriangleCull(
	TEXT("r.RayTracing.DebugDisableTriangleCull"),
	GRayTracingDebugDisableTriangleCull,
	TEXT("Forces all ray tracing geometry instances to be double-sided by disabling back-face culling. This is useful for debugging and profiling. (default = 0)")
);

static int32 GRayTracingDebugForceOpaque = 0;
static FAutoConsoleVariableRef CVarRayTracingDebugForceOpaque(
	TEXT("r.RayTracing.DebugForceOpaque"),
	GRayTracingDebugForceOpaque,
	TEXT("Forces all ray tracing geometry instances to be opaque, effectively disabling any-hit shaders. This is useful for debugging and profiling. (default = 0)")
);

static bool bUpdateCachedRayTracingState = false;

static FAutoConsoleCommand UpdateCachedRayTracingStateCmd(
	TEXT("r.RayTracing.UpdateCachedState"),
	TEXT("Update cached ray tracing state (mesh commands and instances)."),
	FConsoleCommandDelegate::CreateStatic([] { bUpdateCachedRayTracingState = true; }));

static bool bRefreshRayTracingInstances = false;

static void RefreshRayTracingInstancesSinkFunction()
{
	static const auto RayTracingStaticMeshesCVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.RayTracing.Geometry.StaticMeshes"));
	static const auto RayTracingHISMCVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.RayTracing.Geometry.HierarchicalInstancedStaticMesh"));
	static const auto RayTracingNaniteProxiesCVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.RayTracing.Geometry.NaniteProxies"));
	static const auto RayTracingLandscapeGrassCVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.RayTracing.Geometry.LandscapeGrass"));

	static int32 CachedRayTracingStaticMeshes = RayTracingStaticMeshesCVar->GetValueOnGameThread();
	static int32 CachedRayTracingHISM = RayTracingHISMCVar->GetValueOnGameThread();
	static int32 CachedRayTracingNaniteProxies = RayTracingNaniteProxiesCVar->GetValueOnGameThread();
	static int32 CachedRayTracingLandscapeGrass = RayTracingLandscapeGrassCVar->GetValueOnGameThread();

	const int32 RayTracingStaticMeshes = RayTracingStaticMeshesCVar->GetValueOnGameThread();
	const int32 RayTracingHISM = RayTracingHISMCVar->GetValueOnGameThread();
	const int32 RayTracingNaniteProxies = RayTracingNaniteProxiesCVar->GetValueOnGameThread();
	const int32 RayTracingLandscapeGrass = RayTracingLandscapeGrassCVar->GetValueOnGameThread();

	if (RayTracingStaticMeshes != CachedRayTracingStaticMeshes
		|| RayTracingHISM != CachedRayTracingHISM
		|| RayTracingNaniteProxies != CachedRayTracingNaniteProxies
		|| RayTracingLandscapeGrass != CachedRayTracingLandscapeGrass)
	{
		ENQUEUE_RENDER_COMMAND(RefreshRayTracingInstancesCmd)(
			[](FRHICommandListImmediate&)
			{
				bRefreshRayTracingInstances = true;
			}
		);

		CachedRayTracingStaticMeshes = RayTracingStaticMeshes;
		CachedRayTracingHISM = RayTracingHISM;
		CachedRayTracingNaniteProxies = RayTracingNaniteProxies;
		CachedRayTracingLandscapeGrass = RayTracingLandscapeGrass;
	}
}

static FAutoConsoleVariableSink CVarRefreshRayTracingInstancesSink(FConsoleCommandDelegate::CreateStatic(&RefreshRayTracingInstancesSinkFunction));

namespace RayTracing
{
	struct FRelevantPrimitive
	{
		FRHIRayTracingGeometry* RayTracingGeometryRHI = nullptr;
		uint64 StateHash = 0;
		int32 PrimitiveIndex = -1;
		FPersistentPrimitiveIndex PersistentPrimitiveIndex;
		int8 LODIndex = -1;
		uint8 InstanceMask = 0;
		bool bAllSegmentsOpaque : 1 = true;
		bool bAllSegmentsCastShadow : 1 = true;
		bool bAnySegmentsCastShadow : 1 = false;
		bool bAnySegmentsDecal : 1 = false;
		bool bAllSegmentsDecal : 1 = true;
		bool bTwoSided : 1 = false;
		bool bIsSky : 1 = false;
		bool bAllSegmentsTranslucent : 1 = true;
		bool bAllSegmentsReverseCulling : 1 = true;

		const FRayTracingGeometryInstance* CachedRayTracingInstance = nullptr;
		TArrayView<const int32> CachedRayTracingMeshCommandIndices; // Pointer to FPrimitiveSceneInfo::CachedRayTracingMeshCommandIndicesPerLOD data

		// Offsets relative to FRelevantPrimitiveContext offsets
		int32 RelativeInstanceOffset = INDEX_NONE;
		int32 RelativeDecalInstanceOffset = INDEX_NONE;
		int32 RelativeVisibleMeshCommandOffset = INDEX_NONE;
		int32 ContextIndex = INDEX_NONE;

		uint64 InstancingKey() const
		{
			uint64 Key = StateHash;
			Key ^= uint64(InstanceMask) << 32;
			Key ^= bAllSegmentsOpaque ? 0x1ull << 40 : 0x0;
			Key ^= bAllSegmentsCastShadow ? 0x1ull << 41 : 0x0;
			Key ^= bAnySegmentsCastShadow ? 0x1ull << 42 : 0x0;
			Key ^= bAnySegmentsDecal ? 0x1ull << 43 : 0x0;
			Key ^= bAllSegmentsDecal ? 0x1ull << 44 : 0x0;
			Key ^= bTwoSided ? 0x1ull << 45 : 0x0;
			Key ^= bIsSky ? 0x1ull << 46 : 0x0;
			Key ^= bAllSegmentsTranslucent ? 0x1ull << 47 : 0x0;
			Key ^= bAllSegmentsReverseCulling ? 0x1ull << 48 : 0x0;
			return Key ^ reinterpret_cast<uint64>(RayTracingGeometryRHI);
		}

		void FinalizeInstanceMask(const ERayTracingPrimitiveFlags Flags, ERayTracingViewMaskMode MaskMode)
		{
			if (EnumHasAllFlags(Flags, ERayTracingPrimitiveFlags::FarField))
			{
				InstanceMask = ComputeRayTracingInstanceMask(ERayTracingInstanceMaskType::FarField, MaskMode);
			}
		}
	};

	struct FRelevantPrimitiveGatherContext
	{
		int32 InstanceOffset = -1;
		int32 DecalInstanceOffset = -1;
		int32 VisibleMeshCommandOffset = -1;
	};

	struct FRelevantPrimitiveList
	{
		// Filtered lists of relevant primitives
		TArray<FRelevantPrimitive> StaticPrimitives;
		TArray<FRelevantPrimitive> CachedStaticPrimitives;
		TArray<int32> DynamicPrimitives;

		TArray<FRelevantPrimitiveGatherContext> GatherContexts;

		// Relevant static primitive LODs are computed asynchronously.
		// This task must complete before accessing StaticPrimitives/CachedStaticPrimitives in FRayTracingSceneAddInstancesTask.
		FGraphEventRef StaticPrimitiveLODTask;

		// Array of primitives that should update their cached ray tracing instances via FPrimitiveSceneInfo::UpdateCachedRaytracingData()
		TArray<FPrimitiveSceneInfo*> DirtyCachedRayTracingPrimitives; // TODO: remove this since it seems to be transient

		// Used coarse mesh streaming handles during the last TLAS build
		TArray<Nanite::CoarseMeshStreamingHandle> UsedCoarseMeshStreamingHandles; // TODO: Should be a set

		int32 NumCachedStaticInstances = 0;
		int32 NumCachedStaticDecalInstances = 0;
		int32 NumCachedStaticVisibleMeshCommands = 0;

		// Indicates that this object has been fully produced (for validation)
		bool bValid = false;
	};

	void OnRenderBegin(FScene& Scene, TArray<FViewInfo>& Views, const FViewFamilyInfo& ViewFamily)
	{
		const ERayTracingMeshCommandsMode CurrentMode = ViewFamily.EngineShowFlags.PathTracing ? ERayTracingMeshCommandsMode::PATH_TRACING : ERayTracingMeshCommandsMode::RAY_TRACING;
		bool bNaniteCoarseMeshStreamingModeChanged = false;
#if WITH_EDITOR
		bNaniteCoarseMeshStreamingModeChanged = Nanite::FCoarseMeshStreamingManager::CheckStreamingMode();
#endif // WITH_EDITOR
		const bool bNaniteRayTracingModeChanged = Nanite::GRayTracingManager.CheckModeChanged();

		if (CurrentMode != Scene.CachedRayTracingMeshCommandsMode
			|| bNaniteCoarseMeshStreamingModeChanged
			|| bNaniteRayTracingModeChanged
			|| bUpdateCachedRayTracingState)
		{
			Scene.WaitForCacheRayTracingPrimitivesTask();

			// In some situations, we need to refresh the cached ray tracing mesh commands because they contain data about the currently bound shader. 
			// This operation is a bit expensive but only happens once as we transition between modes which should be rare.
			Scene.CachedRayTracingMeshCommandsMode = CurrentMode;
			Scene.RefreshRayTracingMeshCommandCache();
			bUpdateCachedRayTracingState = false;
		}

		if (bRefreshRayTracingInstances)
		{
			Scene.WaitForCacheRayTracingPrimitivesTask();

			// In some situations, we need to refresh the cached ray tracing instance.
			// This assumes that cached instances will keep using the same LOD since CachedRayTracingMeshCommands is not recalculated
			// eg: Need to update PrimitiveRayTracingFlags
			// This operation is a bit expensive but only happens once as we transition between modes which should be rare.
			Scene.RefreshRayTracingInstances();
			bRefreshRayTracingInstances = false;
		}

		if (bNaniteRayTracingModeChanged)
		{
			for (FViewInfo& View : Views)
			{
				if (View.ViewState != nullptr && !View.bIsOfflineRender)
				{
					// don't invalidate in the offline case because we only get one attempt at rendering each sample
					View.ViewState->PathTracingInvalidate();
				}
			}
		}
	}

	FRelevantPrimitiveList* CreateRelevantPrimitiveList(FSceneRenderingBulkObjectAllocator& InAllocator)
	{
		return InAllocator.Create<FRelevantPrimitiveList>();
	}	

	class FRaytracingShaderBindingLayout : public FShaderBindingLayoutContainer
	{
	public:
		static const FShaderBindingLayout& GetInstance(EBindingType BindingType)
		{
			static FRaytracingShaderBindingLayout Instance;
			return Instance.GetLayout(BindingType);
		}
	private:

		FRaytracingShaderBindingLayout()
		{
			// No special binding layout flags required
			EShaderBindingLayoutFlags ShaderBindingLayoutFlags = EShaderBindingLayoutFlags::None;

			// Add scene, view and nanite ray tracing as global/static uniform buffers
			TArray<FShaderParametersMetadata*> StaticUniformBuffers;
			StaticUniformBuffers.Add(FindUniformBufferStructByName(TEXT("Scene")));
			StaticUniformBuffers.Add(FindUniformBufferStructByName(TEXT("View")));
			StaticUniformBuffers.Add(FindUniformBufferStructByName(TEXT("NaniteRayTracing")));
			StaticUniformBuffers.Add(FindUniformBufferStructByName(TEXT("LumenHardwareRayTracingUniformBuffer")));

			BuildShaderBindingLayout(StaticUniformBuffers, ShaderBindingLayoutFlags, *this);
		}
	};

	const FShaderBindingLayout* GetShaderBindingLayout(EShaderPlatform ShaderPlatform)
	{
		if (RHIGetStaticShaderBindingLayoutSupport(ShaderPlatform) != ERHIStaticShaderBindingLayoutSupport::Unsupported)
		{
			// Should support bindless for raytracing at least
			check(RHIGetRuntimeBindlessResourcesConfiguration(ShaderPlatform) != ERHIBindlessConfiguration::Disabled);
			check(RHIGetRuntimeBindlessSamplersConfiguration(ShaderPlatform) != ERHIBindlessConfiguration::Disabled);

			// Retrieve the bindless shader binding table
			return &FRaytracingShaderBindingLayout::GetInstance(FShaderBindingLayoutContainer::EBindingType::Bindless);
		}

		// No binding table supported
		return nullptr;
	}

	TOptional<FScopedUniformBufferStaticBindings> BindStaticUniformBufferBindings(const FViewInfo& View, FRHIUniformBuffer* SceneUniformBuffer, FRHICommandList& RHICmdList)
	{
		TOptional<FScopedUniformBufferStaticBindings> StaticUniformBufferScope;

		// Setup the static uniform buffers used by the RTPSO if enabled
		const FShaderBindingLayout* ShaderBindingLayout = GetShaderBindingLayout(View.GetShaderPlatform());
		if (ShaderBindingLayout)
		{			
			FUniformBufferStaticBindings StaticUniformBuffers(&ShaderBindingLayout->RHILayout);
			StaticUniformBuffers.AddUniformBuffer(View.ViewUniformBuffer.GetReference());
			StaticUniformBuffers.AddUniformBuffer(SceneUniformBuffer);
			StaticUniformBuffers.AddUniformBuffer(Nanite::GRayTracingManager.GetUniformBuffer().GetReference());
			StaticUniformBuffers.AddUniformBuffer(View.GetPrimaryView()->LumenHardwareRayTracingUniformBuffer.GetReference());

			StaticUniformBufferScope.Emplace(RHICmdList, StaticUniformBuffers);
		}

		return StaticUniformBufferScope;
	}

	void GatherRelevantPrimitives(FScene& Scene, const FViewInfo& View, FRelevantPrimitiveList& Result)
	{
		TArray<int32> StaticPrimitives;

		const bool bGameView = View.bIsGameView || View.Family->EngineShowFlags.Game;
		const bool bUsingReferenceBasedResidency = IsRayTracingUsingReferenceBasedResidency();

		bool bPerformRayTracing = View.State != nullptr && !View.bIsReflectionCapture && View.bAllowRayTracing;
		if (bPerformRayTracing)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(GatherRayTracingRelevantPrimitives);

			struct FGatherRelevantPrimitivesContext
			{
				TChunkedArray<int32> StaticPrimitives;
				TChunkedArray<int32> DynamicPrimitives;
				TChunkedArray<Nanite::CoarseMeshStreamingHandle> UsedCoarseMeshStreamingHandles;
				TChunkedArray<FPrimitiveSceneInfo*> DirtyCachedRayTracingPrimitives;

				TSet<RayTracing::GeometryGroupHandle> ReferencedGeometryGroups;
			};

			TArray<FGatherRelevantPrimitivesContext> Contexts;
			const int32 MinBatchSize = 128;
			ParallelForWithTaskContext(
				TEXT("GatherRayTracingRelevantPrimitives_Parallel"),
				Contexts,
				Scene.PrimitiveSceneProxies.Num(),
				MinBatchSize,
				[&Scene, &View, bGameView, bUsingReferenceBasedResidency](FGatherRelevantPrimitivesContext& Context, int32 PrimitiveIndex)
			{
				// Get primitive visibility state from culling
				if (!View.PrimitiveRayTracingVisibilityMap[PrimitiveIndex])
				{
					return;
				}

				const ERayTracingPrimitiveFlags Flags = Scene.PrimitiveRayTracingFlags[PrimitiveIndex];

				check(!EnumHasAnyFlags(Flags, ERayTracingPrimitiveFlags::Exclude));

				const FPrimitiveSceneInfo* SceneInfo = Scene.Primitives[PrimitiveIndex];

				// #dxr_todo: ray tracing in scene captures should re-use the persistent RT scene. (UE-112448)
				bool bShouldRayTraceSceneCapture = GRayTracingSceneCaptures > 0
					|| (GRayTracingSceneCaptures == -1 && View.bSceneCaptureUsesRayTracing);

				if (View.bIsSceneCapture && (!bShouldRayTraceSceneCapture || !SceneInfo->bIsVisibleInSceneCaptures))
				{
					return;
				}

				if (!View.bIsSceneCapture && SceneInfo->bIsVisibleInSceneCapturesOnly)
				{
					return;
				}

				// Some primitives should only be visible editor mode, however far field geometry 
				// and hidden shadow casters must still always be added to the RT scene.
				if (bGameView && !SceneInfo->bDrawInGame && !SceneInfo->bRayTracingFarField)
				{
					// Make sure this isn't an object that wants to be hidden to camera but still wants to cast shadows or be visible to indirect
					checkf(SceneInfo->Proxy != nullptr, TEXT("SceneInfo does not have a valid Proxy object. If this occurs, this object should probably have been filtered out before being added to Scene.Primitives"));
					if (!SceneInfo->Proxy->CastsHiddenShadow() && !SceneInfo->Proxy->AffectsIndirectLightingWhileHidden())
					{
						return;
					}
				}

				// Marked visible and used after point, check if streaming then mark as used in the TLAS (so it can be streamed in)
				if (EnumHasAnyFlags(Flags, ERayTracingPrimitiveFlags::Streaming))
				{
					check(SceneInfo->CoarseMeshStreamingHandle != INDEX_NONE);
					Context.UsedCoarseMeshStreamingHandles.AddElement(SceneInfo->CoarseMeshStreamingHandle);
				}

				if (bUsingReferenceBasedResidency && SceneInfo->RayTracingGeometryGroupHandle != INDEX_NONE)
				{
					Context.ReferencedGeometryGroups.Add(SceneInfo->RayTracingGeometryGroupHandle);
				}

				// Is the cached data dirty?
				// eg: mesh was streamed in/out
				if (SceneInfo->bCachedRaytracingDataDirty)
				{
					Context.DirtyCachedRayTracingPrimitives.AddElement(Scene.Primitives[PrimitiveIndex]);
				}

				if (EnumHasAnyFlags(Flags, ERayTracingPrimitiveFlags::Skip))
				{
					return;
				}

				if (EnumHasAnyFlags(Flags, ERayTracingPrimitiveFlags::Dynamic))
				{
					checkf(!EnumHasAllFlags(Flags, ERayTracingPrimitiveFlags::CacheInstances), TEXT("Only static primitives are expected to use CacheInstances flag."));

					if (View.Family->EngineShowFlags.SkeletalMeshes) // TODO: Fix this check
					{
						Context.DynamicPrimitives.AddElement(PrimitiveIndex);
					}
				}
				else if (View.Family->EngineShowFlags.StaticMeshes)
				{
					Context.StaticPrimitives.AddElement(PrimitiveIndex);
				}
			});

			if (Contexts.Num() > 0)
			{
				SCOPED_NAMED_EVENT(GatherRayTracingRelevantPrimitives_Merge, FColor::Emerald);

				int32 NumStaticPrimitives = 0;
				int32 NumDynamicPrimitives = 0;
				int32 NumUsedCoarseMeshStreamingHandles = 0;
				int32 NumDirtyCachedRayTracingPrimitives = 0;

				for (auto& Context : Contexts)
				{
					NumStaticPrimitives += Context.StaticPrimitives.Num();
					NumDynamicPrimitives += Context.DynamicPrimitives.Num();
					NumUsedCoarseMeshStreamingHandles += Context.UsedCoarseMeshStreamingHandles.Num();
					NumDirtyCachedRayTracingPrimitives += Context.DirtyCachedRayTracingPrimitives.Num();
				}

				StaticPrimitives.Reserve(NumStaticPrimitives);
				Result.DynamicPrimitives.Reserve(NumDynamicPrimitives);
				Result.UsedCoarseMeshStreamingHandles.Reserve(NumUsedCoarseMeshStreamingHandles);
				Result.DirtyCachedRayTracingPrimitives.Reserve(NumDirtyCachedRayTracingPrimitives);

				for (auto& Context : Contexts)
				{
					Context.StaticPrimitives.CopyToLinearArray(StaticPrimitives);
					Context.DynamicPrimitives.CopyToLinearArray(Result.DynamicPrimitives);
					Context.UsedCoarseMeshStreamingHandles.CopyToLinearArray(Result.UsedCoarseMeshStreamingHandles);
					Context.DirtyCachedRayTracingPrimitives.CopyToLinearArray(Result.DirtyCachedRayTracingPrimitives);

					if(bUsingReferenceBasedResidency)
					{
						((FRayTracingGeometryManager*)GRayTracingGeometryManager)->AddReferencedGeometryGroups(Context.ReferencedGeometryGroups);
					}
				}
			}
		}

		// TODO: check whether it's ok to do this on a parallel task
		FPrimitiveSceneInfo::UpdateCachedRaytracingData(&Scene, Result.DirtyCachedRayTracingPrimitives);

		static const auto ICVarStaticMeshLODDistanceScale = IConsoleManager::Get().FindConsoleVariable(TEXT("r.StaticMeshLODDistanceScale"));
		const float LODScaleCVarValue = ICVarStaticMeshLODDistanceScale->GetFloat();
		const int32 ForcedLODLevel = GetCVarForceLOD();

		Result.StaticPrimitiveLODTask = FFunctionGraphTask::CreateAndDispatchWhenReady(
			[&Result, &Scene, &View, LODScaleCVarValue, ForcedLODLevel, StaticPrimitiveIndices = MoveTemp(StaticPrimitives), bUsingReferenceBasedResidency]()
			{
				FTaskTagScope TaskTagScope(ETaskTag::EParallelRenderingThread);

				TRACE_CPUPROFILER_EVENT_SCOPE(GatherRayTracingRelevantPrimitives_ComputeLOD);

				struct FRelevantStaticPrimitivesContext
				{
					FRelevantStaticPrimitivesContext(int32 InContextIndex) : ContextIndex(InContextIndex) {}

					TChunkedArray<FRelevantPrimitive> StaticPrimitives;
					TChunkedArray<FRelevantPrimitive> CachedStaticPrimitives;
					TChunkedArray<const FPrimitiveSceneInfo*> VisibleNaniteRayTracingPrimitives;

					int32 NumCachedStaticInstances = 0;
					int32 NumCachedStaticDecalInstances = 0;
					int32 NumCachedStaticVisibleMeshCommands = 0;

					int32 ContextIndex = INDEX_NONE;
				};

				TArray<FRelevantStaticPrimitivesContext> Contexts;
				ParallelForWithTaskContext(
					TEXT("GatherRayTracingRelevantPrimitives_ComputeLOD_Parallel"),
					Contexts,
					StaticPrimitiveIndices.Num(),
					[](int32 ContextIndex, int32 NumContexts) { return ContextIndex; },
					[&Scene, &View, LODScaleCVarValue, ForcedLODLevel, &StaticPrimitiveIndices, bUsingReferenceBasedResidency](FRelevantStaticPrimitivesContext& Context, int32 ItemIndex)
					{
						const int32 PrimitiveIndex = StaticPrimitiveIndices[ItemIndex];

						const FPrimitiveSceneProxy* SceneProxy = Scene.PrimitiveSceneProxies[PrimitiveIndex];
						const FPrimitiveSceneInfo* SceneInfo = Scene.Primitives[PrimitiveIndex];
						const ERayTracingPrimitiveFlags Flags = Scene.PrimitiveRayTracingFlags[PrimitiveIndex];

						const bool bUsingNaniteRayTracing = (Nanite::GetRayTracingMode() != Nanite::ERayTracingMode::Fallback) && SceneProxy->IsNaniteMesh();

						if (bUsingNaniteRayTracing)
						{
							Context.VisibleNaniteRayTracingPrimitives.AddElement(SceneInfo);
						}

						int8 LODIndex = 0;

						if (EnumHasAnyFlags(Flags, ERayTracingPrimitiveFlags::ComputeLOD))
						{
							const FPrimitiveBounds& Bounds = Scene.PrimitiveBounds[PrimitiveIndex];

							const int8 CurFirstLODIdx = SceneProxy->GetCurrentFirstLODIdx_RenderThread();
							check(CurFirstLODIdx >= 0);

							float MeshScreenSizeSquared = 0;
							float LODScale = LODScaleCVarValue * View.LODDistanceFactor;
							FLODMask LODToRender = ComputeLODForMeshes(SceneInfo->StaticMeshRelevances, View, Bounds.BoxSphereBounds.Origin, Bounds.BoxSphereBounds.SphereRadius, ForcedLODLevel, MeshScreenSizeSquared, CurFirstLODIdx, LODScale, true);

							LODIndex = LODToRender.GetRayTracedLOD();
						}

						if (EnumHasAnyFlags(Flags, ERayTracingPrimitiveFlags::CacheInstances))
						{
							if (bUsingNaniteRayTracing)
							{
								if (SceneInfo->CachedRayTracingInstance.GeometryRHI == nullptr)
								{
									// Nanite ray tracing geometry not ready yet, doesn't include primitive in ray tracing scene
									return;
								}
							}
							else
							{
								// Currently IsCachedRayTracingGeometryValid() can only be called for non-nanite geometries
								checkf(SceneInfo->IsCachedRayTracingGeometryValid(), TEXT("Cached ray tracing instance is expected to be valid. Was mesh LOD streamed but cached data was not invalidated?"));
								checkf(SceneInfo->CachedRayTracingInstance.GeometryRHI, TEXT("Ray tracing instance must have a valid geometry."));
							}

							if (ShouldExcludeDecals() && SceneInfo->bCachedRayTracingInstanceAllSegmentsDecal)
							{
								return;
							}

							checkf(SceneInfo->CachedRayTracingInstance.GeometryRHI, TEXT("Ray tracing instance must have a valid geometry."));

							// For primitives with ERayTracingPrimitiveFlags::CacheInstances flag we only cache the instance/mesh commands of the current LOD
							// (see FPrimitiveSceneInfo::UpdateCachedRayTracingInstance(...) and CacheRayTracingPrimitive(...))
							check(!EnumHasAnyFlags(Flags, ERayTracingPrimitiveFlags::ComputeLOD));
							LODIndex = 0;

							FRelevantPrimitive* RelevantPrimitive = new (Context.CachedStaticPrimitives) FRelevantPrimitive();
							RelevantPrimitive->PrimitiveIndex = PrimitiveIndex;
							RelevantPrimitive->PersistentPrimitiveIndex = SceneInfo->GetPersistentIndex();

							ensureMsgf(!SceneInfo->bCachedRaytracingDataDirty, TEXT("Cached ray tracing instances must be up-to-date at this point"));

							RelevantPrimitive->CachedRayTracingInstance = &SceneInfo->CachedRayTracingInstance;
							RelevantPrimitive->bAnySegmentsDecal = SceneInfo->bCachedRayTracingInstanceAnySegmentsDecal;
							RelevantPrimitive->bAllSegmentsDecal = SceneInfo->bCachedRayTracingInstanceAllSegmentsDecal;

							if (SceneInfo->CachedRayTracingMeshCommandIndicesPerLOD.IsValidIndex(LODIndex))
							{
								RelevantPrimitive->CachedRayTracingMeshCommandIndices = SceneInfo->CachedRayTracingMeshCommandIndicesPerLOD[LODIndex];
							}

							// CacheInstances expects to have one ray tracing mesh command per BLAS segment.
							// If that's not the case in the future, other logic such as NumCachedStaticVisibleMeshCommands calculation needs to be updated.
							checkf(RelevantPrimitive->CachedRayTracingMeshCommandIndices.Num() == SceneInfo->CachedRayTracingInstance.GeometryRHI->GetNumSegments(),
								TEXT("Expected to have one ray tracing mesh command per BLAS segment (primitive has %d cached mesh commands but BLAS has %d segments)."),
								RelevantPrimitive->CachedRayTracingMeshCommandIndices.Num(), SceneInfo->CachedRayTracingInstance.GeometryRHI->GetNumSegments());

							const bool bNeedMainInstance = !RelevantPrimitive->bAllSegmentsDecal;

							// if primitive has mixed decal and non-decal segments we need to have two ray tracing instances
							// one containing non-decal segments and the other with decal segments
							// masking of segments is done using "hidden" hitgroups
							// TODO: Debug Visualization to highlight primitives using this?
							const bool bNeedDecalInstance = RelevantPrimitive->bAnySegmentsDecal && !ShouldExcludeDecals();

							checkf(bNeedMainInstance || bNeedDecalInstance, TEXT("FRelevantPrimitive is expected to have a main instance, decal instance or both."));
							const uint32 NumTLASInstances = bNeedMainInstance && bNeedDecalInstance ? 2 : 1;

							// For now store offsets relative to current context
							// Will be patched later to be a global offset
							RelevantPrimitive->RelativeInstanceOffset = Context.NumCachedStaticInstances;
							RelevantPrimitive->RelativeDecalInstanceOffset = Context.NumCachedStaticDecalInstances;
							RelevantPrimitive->RelativeVisibleMeshCommandOffset = Context.NumCachedStaticVisibleMeshCommands;
							RelevantPrimitive->ContextIndex = Context.ContextIndex;

							if (bNeedMainInstance)
							{
								++Context.NumCachedStaticInstances;
							}

							if (bNeedDecalInstance)
							{
								++Context.NumCachedStaticDecalInstances;
							}

							Context.NumCachedStaticVisibleMeshCommands += RelevantPrimitive->CachedRayTracingMeshCommandIndices.Num() * NumTLASInstances;
						}
						// - DirtyCachedRayTracingPrimitives are only processed after StaticPrimitiveIndices is filled
						// so we can end up with primitives that should be skipped here
						// - once we update flags of primitive with dirty raytracing state before `GatherRayTracingRelevantPrimitives_Parallel`
						// we should replace this condition with an assert instead
						else if(!EnumHasAnyFlags(Flags, ERayTracingPrimitiveFlags::Skip))
						{
#if DO_CHECK
							if(bUsingReferenceBasedResidency)
							{
								FRayTracingGeometry* TargetRayTracingGeometry = SceneInfo->GetStaticRayTracingGeometry(LODIndex);
								if (!ensure(TargetRayTracingGeometry != nullptr))
								{
									return;
								}

								ensure(((FRayTracingGeometryManager*)GRayTracingGeometryManager)->IsGeometryGroupReferenced(TargetRayTracingGeometry->GroupHandle));

							}
#endif

							FRayTracingGeometry* RayTracingGeometry = SceneInfo->GetValidStaticRayTracingGeometry(LODIndex);

							if (RayTracingGeometry == nullptr)
							{
								return;
							}

							check(RayTracingGeometry->LODIndex == LODIndex);

							// Sometimes LODIndex is out of range because it is clamped by ClampToFirstLOD, like the requested LOD is being streamed in and hasn't been available
							// According to InitViews, we should hide the static mesh instance
							if (SceneInfo->CachedRayTracingMeshCommandIndicesPerLOD.IsValidIndex(LODIndex))
							{
								FRelevantPrimitive* RelevantPrimitive = new (Context.StaticPrimitives) FRelevantPrimitive();
								RelevantPrimitive->PrimitiveIndex = PrimitiveIndex;
								RelevantPrimitive->PersistentPrimitiveIndex = SceneInfo->GetPersistentIndex();

								RelevantPrimitive->LODIndex = LODIndex;
								RelevantPrimitive->RayTracingGeometryRHI = RayTracingGeometry->GetRHI();

								RelevantPrimitive->CachedRayTracingMeshCommandIndices = SceneInfo->CachedRayTracingMeshCommandIndicesPerLOD[LODIndex];
								RelevantPrimitive->StateHash = SceneInfo->CachedRayTracingMeshCommandsHashPerLOD[LODIndex];

								const ERayTracingViewMaskMode MaskMode = static_cast<ERayTracingViewMaskMode>(Scene.CachedRayTracingMeshCommandsMode);

								// TODO: Cache these flags to avoid having to loop over the RayTracingMeshCommands
								for (int32 CommandIndex : RelevantPrimitive->CachedRayTracingMeshCommandIndices)
								{
									if (CommandIndex >= 0)
									{
										const FRayTracingMeshCommand& RayTracingMeshCommand = Scene.CachedRayTracingMeshCommands[CommandIndex];

										RelevantPrimitive->InstanceMask |= RayTracingMeshCommand.InstanceMask;
										RelevantPrimitive->bAllSegmentsOpaque &= RayTracingMeshCommand.bOpaque;
										RelevantPrimitive->bAllSegmentsCastShadow &= RayTracingMeshCommand.bCastRayTracedShadows;
										RelevantPrimitive->bAnySegmentsCastShadow |= RayTracingMeshCommand.bCastRayTracedShadows;
										RelevantPrimitive->bAnySegmentsDecal |= RayTracingMeshCommand.bDecal;
										RelevantPrimitive->bAllSegmentsDecal &= RayTracingMeshCommand.bDecal;
										RelevantPrimitive->bTwoSided |= RayTracingMeshCommand.bTwoSided;
										RelevantPrimitive->bIsSky |= RayTracingMeshCommand.bIsSky;
										RelevantPrimitive->bAllSegmentsTranslucent &= RayTracingMeshCommand.bIsTranslucent;
										RelevantPrimitive->bAllSegmentsReverseCulling &= RayTracingMeshCommand.bReverseCulling;
									}
									else
									{
										// CommandIndex == -1 indicates that the mesh batch has been filtered by FRayTracingMeshProcessor (like the shadow depth pass batch)
										// Do nothing in this case
									}
								}

								RelevantPrimitive->FinalizeInstanceMask(Flags, MaskMode);
							}
						}
					});

					if (Contexts.Num() > 0)
					{
						SCOPED_NAMED_EVENT(GatherRayTracingRelevantPrimitives_ComputeLOD_Merge, FColor::Emerald);

						uint32 NumStaticPrimitives = 0;
						uint32 NumCachedStaticPrimitives = 0;

						for (auto& Context : Contexts)
						{
							NumStaticPrimitives += Context.StaticPrimitives.Num();
							NumCachedStaticPrimitives += Context.CachedStaticPrimitives.Num();
						}

						Result.StaticPrimitives.Reserve(NumStaticPrimitives);
						Result.CachedStaticPrimitives.Reserve(NumCachedStaticPrimitives);

						Result.GatherContexts.SetNum(Contexts.Num());

						for (int32 ContextIndex = 0; ContextIndex < Contexts.Num(); ++ContextIndex)
						{
							FRelevantStaticPrimitivesContext& Context = Contexts[ContextIndex];
							FRelevantPrimitiveGatherContext& GatherContext = Result.GatherContexts[ContextIndex];

							Context.StaticPrimitives.CopyToLinearArray(Result.StaticPrimitives);
							Context.CachedStaticPrimitives.CopyToLinearArray(Result.CachedStaticPrimitives);

							GatherContext.InstanceOffset = Result.NumCachedStaticInstances;
							GatherContext.DecalInstanceOffset = Result.NumCachedStaticDecalInstances;
							GatherContext.VisibleMeshCommandOffset = Result.NumCachedStaticVisibleMeshCommands;

							Result.NumCachedStaticInstances += Context.NumCachedStaticInstances;
							Result.NumCachedStaticDecalInstances += Context.NumCachedStaticDecalInstances;
							Result.NumCachedStaticVisibleMeshCommands += Context.NumCachedStaticVisibleMeshCommands;

							for (const FPrimitiveSceneInfo* SceneInfo : Context.VisibleNaniteRayTracingPrimitives)
							{
								Nanite::GRayTracingManager.AddVisiblePrimitive(SceneInfo);
							}
						}
					}
			}, TStatId(), nullptr, ENamedThreads::AnyThread);

		Result.bValid = true;
	}

	static void AddDebugRayTracingInstanceFlags(ERayTracingInstanceFlags& InOutFlags)
	{
		if (GRayTracingDebugForceOpaque)
		{
			InOutFlags |= ERayTracingInstanceFlags::ForceOpaque;
		}
		if (GRayTracingDebugDisableTriangleCull)
		{
			InOutFlags |= ERayTracingInstanceFlags::TriangleCullDisable;
		}
	}

	// Configure ray tracing scene options based on currently enabled features and their needs
	FSceneOptions::FSceneOptions(
		FScene& Scene,
		const FViewFamilyInfo& ViewFamily,
		FViewInfo& View,
		EDiffuseIndirectMethod DiffuseIndirectMethod,
		EReflectionsMethod ReflectionsMethod,
		RayTracing::FSceneOptions& SceneOptions)
	{
		SceneOptions.bTranslucentGeometry = false;
		LumenHardwareRayTracing::SetRayTracingSceneOptions(View, DiffuseIndirectMethod, ReflectionsMethod, SceneOptions);
		RayTracingShadows::SetRayTracingSceneOptions(Scene.bHasLightsWithRayTracedShadows, SceneOptions);

		if (ShouldRenderRayTracingTranslucency(View))
		{
			SceneOptions.bTranslucentGeometry = true;
		}

		if (ViewFamily.EngineShowFlags.PathTracing
			&& FDataDrivenShaderPlatformInfo::GetSupportsPathTracing(Scene.GetShaderPlatform()))
		{
			SceneOptions.bTranslucentGeometry = true;
		}

		if (GRayTracingExcludeTranslucent != 0)
		{
			SceneOptions.bTranslucentGeometry = false;
		}
	}

	bool GatherWorldInstancesForView(
		FRDGBuilder& GraphBuilder,
		FScene& Scene,
		const FViewFamilyInfo& ViewFamily,
		FViewInfo& View,
		EDiffuseIndirectMethod DiffuseIndirectMethod,
		EReflectionsMethod ReflectionsMethod,
		FRayTracingScene& RayTracingScene,
		FGlobalDynamicReadBuffer& InDynamicReadBuffer,
		FSceneRenderingBulkObjectAllocator& InBulkAllocator,
		FRelevantPrimitiveList& RelevantPrimitiveList)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(GatherRayTracingWorldInstances);
		SCOPE_CYCLE_COUNTER(STAT_GatherRayTracingWorldInstances);

		// Prepare ray tracing scene instance list
		checkf(RelevantPrimitiveList.bValid, TEXT("Ray tracing relevant primitive list is expected to have been created before GatherRayTracingWorldInstancesForView() is called."));

		// Check that any invalidated cached uniform expressions have been updated on the rendering thread.
		// Normally this work is done through FMaterialRenderProxy::UpdateUniformExpressionCacheIfNeeded,
		// however ray tracing material processing (FMaterialShader::GetShaderBindings, which accesses UniformExpressionCache)
		// is done on task threads, therefore all work must be done here up-front as UpdateUniformExpressionCacheIfNeeded is not free-threaded.
		check(!FMaterialRenderProxy::HasDeferredUniformExpressionCacheRequests());

		FGPUScenePrimitiveCollector DummyDynamicPrimitiveCollector;

		View.DynamicRayTracingMeshCommandStorage.Reserve(Scene.Primitives.Num());
		View.VisibleRayTracingMeshCommands.Reserve(Scene.Primitives.Num());

		View.RayTracingMeshResourceCollector = MakeUnique<FRayTracingMeshResourceCollector>(Scene.GetFeatureLevel(), InBulkAllocator);

		View.RayTracingCullingParameters.Init(View);

		RayTracing::FSceneOptions SceneOptions(Scene, ViewFamily, View, DiffuseIndirectMethod, ReflectionsMethod, SceneOptions);

		const float CurrentWorldTime = View.Family->Time.GetWorldTimeSeconds();

		// Consume output of the relevant primitive gathering task
		RayTracingScene.UsedCoarseMeshStreamingHandles = MoveTemp(RelevantPrimitiveList.UsedCoarseMeshStreamingHandles);

		// Inform the coarse mesh streaming manager about all the used streamable render assets in the scene
		Nanite::FCoarseMeshStreamingManager* CoarseMeshSM = IStreamingManager::Get().GetNaniteCoarseMeshStreamingManager();
		if (CoarseMeshSM)
		{
			CoarseMeshSM->AddUsedStreamingHandles(RayTracingScene.UsedCoarseMeshStreamingHandles);
		}

		INC_DWORD_STAT_BY(STAT_VisibleRayTracingPrimitives, RelevantPrimitiveList.DynamicPrimitives.Num() + RelevantPrimitiveList.StaticPrimitives.Num());

		{
			TRACE_CPUPROFILER_EVENT_SCOPE(GatherRayTracingWorldInstances_DynamicElements);

			const bool bParallelMeshBatchSetup = GRayTracingParallelMeshBatchSetup && FApp::ShouldUseThreadingForPerformance();
			const bool bUsingReferenceBasedResidency = IsRayTracingUsingReferenceBasedResidency();

			const int64 SharedBufferGenerationID = Scene.GetRayTracingDynamicGeometryCollection()->BeginUpdate();

			struct FRayTracingMeshBatchWorkItem
			{
				const FPrimitiveSceneProxy* SceneProxy = nullptr;
				const FRHIRayTracingGeometry* RayTracingGeometry = nullptr;
				TArray<FMeshBatch> MeshBatchesOwned;
				TArrayView<const FMeshBatch> MeshBatchesView;
				uint32 GlobalSegmentIndex;
				uint32 DecalGlobalSegmentIndex;

				TArrayView<const FMeshBatch> GetMeshBatches() const
				{
					if (MeshBatchesOwned.Num())
					{
						check(MeshBatchesView.Num() == 0);
						return TArrayView<const FMeshBatch>(MeshBatchesOwned);
					}
					else
					{
						check(MeshBatchesOwned.Num() == 0);
						return MeshBatchesView;
					}
				}
			};

			static constexpr uint32 MaxWorkItemsPerPage = 128; // Try to keep individual pages small to avoid slow-path memory allocations
			struct FRayTracingMeshBatchTaskPage
			{
				FRayTracingMeshBatchWorkItem WorkItems[MaxWorkItemsPerPage];
				uint32 NumWorkItems = 0;
				FRayTracingMeshBatchTaskPage* Next = nullptr;
			};

			FRayTracingMeshBatchTaskPage* MeshBatchTaskHead = nullptr;
			FRayTracingMeshBatchTaskPage* MeshBatchTaskPage = nullptr;
			uint32 NumPendingMeshBatches = 0;
			const uint32 RayTracingParallelMeshBatchSize = GRayTracingParallelMeshBatchSize;

			auto KickRayTracingMeshBatchTask = [&InBulkAllocator, &View, &Scene, &MeshBatchTaskHead, &MeshBatchTaskPage, &NumPendingMeshBatches]()
				{
					if (MeshBatchTaskHead)
					{
						FDynamicRayTracingMeshCommandStorage* TaskDynamicCommandStorage = InBulkAllocator.Create<FDynamicRayTracingMeshCommandStorage>();
						View.DynamicRayTracingMeshCommandStoragePerTask.Add(TaskDynamicCommandStorage);

						FRayTracingMeshCommandOneFrameArray* TaskVisibleCommands = InBulkAllocator.Create<FRayTracingMeshCommandOneFrameArray>();
						TaskVisibleCommands->Reserve(NumPendingMeshBatches);
						View.VisibleRayTracingMeshCommandsPerTask.Add(TaskVisibleCommands);

						View.AddRayTracingMeshBatchTaskList.Add(FFunctionGraphTask::CreateAndDispatchWhenReady(
							[TaskDataHead = MeshBatchTaskHead, &View, &Scene, TaskDynamicCommandStorage, TaskVisibleCommands]()
							{
								FTaskTagScope TaskTagScope(ETaskTag::EParallelRenderingThread);
								TRACE_CPUPROFILER_EVENT_SCOPE(RayTracingMeshBatchTask);
								FRayTracingMeshBatchTaskPage* Page = TaskDataHead;
								const int32 ExpectedMaxVisibieCommands = TaskVisibleCommands->Max();
								while (Page)
								{
									for (uint32 ItemIndex = 0; ItemIndex < Page->NumWorkItems; ++ItemIndex)
									{
										const FRayTracingMeshBatchWorkItem& WorkItem = Page->WorkItems[ItemIndex];
										TArrayView<const FMeshBatch> MeshBatches = WorkItem.GetMeshBatches();
										for (int32 SegmentIndex = 0; SegmentIndex < MeshBatches.Num(); SegmentIndex++)
										{
											const FMeshBatch& MeshBatch = MeshBatches[SegmentIndex];
											FDynamicRayTracingMeshCommandContext CommandContext(
												*TaskDynamicCommandStorage, *TaskVisibleCommands,
												WorkItem.RayTracingGeometry, SegmentIndex, WorkItem.GlobalSegmentIndex, WorkItem.DecalGlobalSegmentIndex);
											FRayTracingMeshProcessor RayTracingMeshProcessor(&CommandContext, &Scene, &View, Scene.CachedRayTracingMeshCommandsMode);
											RayTracingMeshProcessor.AddMeshBatch(MeshBatch, 1, WorkItem.SceneProxy);
										}
									}
									FRayTracingMeshBatchTaskPage* NextPage = Page->Next;
									Page = NextPage;
								}
								check(ExpectedMaxVisibieCommands <= TaskVisibleCommands->Max());
							}, TStatId(), nullptr, ENamedThreads::AnyThread));
					}

					MeshBatchTaskHead = nullptr;
					MeshBatchTaskPage = nullptr;
					NumPendingMeshBatches = 0;
				};

			// Need to process dynamic primitives in 3 passes to support dynamic primitives/instances in GPU Scene
			// 1 - gather all dynamic ray tracing instances
			// 2 - upload dynamic primitive/instance data to GPU scene
			// 3 - process dynamic ray tracing instances (primitive IDs and instance scene data offset of dynamic primitives is now valid)

			// Local temporary array of instances used for GetDynamicRayTracingInstances()
			TArray<FRayTracingInstance> DynamicRayTracingInstances;
			TArray<TRange<int32>> PrimitivesDynamicRayTracingInstances;

			{
				FRayTracingMaterialGatheringContext MaterialGatheringContext
				(
					&Scene,
					&View,
					*View.Family,
					GraphBuilder,
					*View.RayTracingMeshResourceCollector,
					View.RayTracingDynamicPrimitiveCollector,
					InDynamicReadBuffer
				);

				for (int32 PrimitiveIndex : RelevantPrimitiveList.DynamicPrimitives)
				{
					check(MaterialGatheringContext.DynamicRayTracingGeometriesToUpdate.IsEmpty());

					FPrimitiveSceneProxy* SceneProxy = Scene.PrimitiveSceneProxies[PrimitiveIndex];
					FPrimitiveSceneInfo* SceneInfo = Scene.Primitives[PrimitiveIndex];
					const FPersistentPrimitiveIndex PersistentPrimitiveIndex = SceneInfo->GetPersistentIndex();

					MaterialGatheringContext.SetPrimitive(SceneProxy);

					int32 BaseRayTracingInstance = DynamicRayTracingInstances.Num();

					if (SceneOptions.bTranslucentGeometry || SceneProxy->IsOpaqueOrMasked())
					{
						SceneProxy->GetDynamicRayTracingInstances(MaterialGatheringContext, DynamicRayTracingInstances);
					}

					for (const FRayTracingDynamicGeometryUpdateParams& DynamicRayTracingGeometryUpdate : MaterialGatheringContext.DynamicRayTracingGeometriesToUpdate)
					{
						Scene.GetRayTracingDynamicGeometryCollection()->AddDynamicMeshBatchForGeometryUpdate(
							GraphBuilder.RHICmdList,
							&Scene,
							&View,
							SceneProxy,
							DynamicRayTracingGeometryUpdate,
							PersistentPrimitiveIndex.Index
						);
					}

					if (bUsingReferenceBasedResidency)
					{
						((FRayTracingGeometryManager*)GRayTracingGeometryManager)->AddReferencedGeometryGroups(MaterialGatheringContext.GetReferencedGeometryGroups());
					}

					MaterialGatheringContext.Reset();

					PrimitivesDynamicRayTracingInstances.Add(TRange<int32>(BaseRayTracingInstance, DynamicRayTracingInstances.Num()));
				}

				// FRayTracingMaterialGatheringContext destructor handles committing dynamic mesh batches to GPU Scene
			}

			Scene.GPUScene.UploadDynamicPrimitiveShaderDataForView(GraphBuilder, View, true);

			const int32 ViewDynamicPrimitiveId = View.RayTracingDynamicPrimitiveCollector.GetPrimitiveIdRange().GetLowerBoundValue();
			const int32 ViewInstanceSceneDataOffset = View.RayTracingDynamicPrimitiveCollector.GetInstanceSceneDataOffset();

			for (int32 Index = 0; Index < RelevantPrimitiveList.DynamicPrimitives.Num(); ++Index)
			{
				const int32 PrimitiveIndex = RelevantPrimitiveList.DynamicPrimitives[Index];
				FPrimitiveSceneProxy* SceneProxy = Scene.PrimitiveSceneProxies[PrimitiveIndex];
				FPrimitiveSceneInfo* SceneInfo = Scene.Primitives[PrimitiveIndex];
				const FPersistentPrimitiveIndex PersistentPrimitiveIndex = SceneInfo->GetPersistentIndex();

				TArrayView<FRayTracingInstance> TempRayTracingInstances = MakeArrayView(
					DynamicRayTracingInstances.GetData() + PrimitivesDynamicRayTracingInstances[Index].GetLowerBound().GetValue(),
					PrimitivesDynamicRayTracingInstances[Index].Size<int32>());

				if (TempRayTracingInstances.Num() > 0)
				{
					for (FRayTracingInstance& Instance : TempRayTracingInstances)
					{
						const FRayTracingGeometry* Geometry = Instance.Geometry;

						if (!ensureMsgf(Geometry->DynamicGeometrySharedBufferGenerationID == FRayTracingGeometry::NonSharedVertexBuffers
							|| Geometry->DynamicGeometrySharedBufferGenerationID == SharedBufferGenerationID,
							TEXT("GenerationID %lld, but expected to be %lld or %lld. Geometry debug name: '%s'. ")
							TEXT("When shared vertex buffers are used, the contents is expected to be written every frame. ")
							TEXT("Possibly AddDynamicMeshBatchForGeometryUpdate() was not called for this geometry."),
							Geometry->DynamicGeometrySharedBufferGenerationID, SharedBufferGenerationID, FRayTracingGeometry::NonSharedVertexBuffers,
							*Geometry->Initializer.DebugName.ToString()))
						{
							continue;
						}

						((FRayTracingGeometryManager*)GRayTracingGeometryManager)->AddReferencedGeometry(Geometry);

						if (Geometry->IsEvicted())
						{
							continue;
						}

						// If geometry still has pending build request then add to list which requires a force build
						if (Geometry->HasPendingBuildRequest())
						{
							RayTracingScene.GeometriesToBuild.Add(Geometry);
						}

						// Validate the material/segment counts
						if (!ensureMsgf(Instance.GetMaterials().Num() == Geometry->Initializer.Segments.Num() ||
							(Geometry->Initializer.Segments.Num() == 0 && Instance.GetMaterials().Num() == 1),
							TEXT("Ray tracing material assignment validation failed for geometry '%s'. "
								"Instance.GetMaterials().Num() = %d, Geometry->Initializer.Segments.Num() = %d."),
							*Geometry->Initializer.DebugName.ToString(), Instance.GetMaterials().Num(),
							Geometry->Initializer.Segments.Num()))
						{
							continue;
						}

						if (Instance.bInstanceMaskAndFlagsDirty || SceneInfo->bCachedRayTracingInstanceMaskAndFlagsDirty)
						{
							// Build InstanceMaskAndFlags since the data in SceneInfo is not up to date

							FRayTracingMaskAndFlags InstanceMaskAndFlags;

							if (Instance.GetMaterials().IsEmpty())
							{
								// If the material list is empty, explicitly set the mask to 0 so it will not be added in the raytracing scene
								InstanceMaskAndFlags.Mask = 0;
							}
							else
							{
								InstanceMaskAndFlags = BuildRayTracingInstanceMaskAndFlags(Instance, *SceneProxy);
							}

							SceneInfo->CachedRayTracingInstance.Mask = InstanceMaskAndFlags.Mask; // When no cached command is found, InstanceMask == 0 and the instance is effectively filtered out

							if (InstanceMaskAndFlags.bForceOpaque)
							{
								SceneInfo->CachedRayTracingInstance.Flags |= ERayTracingInstanceFlags::ForceOpaque;
							}

							if (InstanceMaskAndFlags.bDoubleSided)
							{
								SceneInfo->CachedRayTracingInstance.Flags |= ERayTracingInstanceFlags::TriangleCullDisable;
							}

							if (InstanceMaskAndFlags.bReverseCulling)
							{
								SceneInfo->CachedRayTracingInstance.Flags |= ERayTracingInstanceFlags::TriangleCullReverse;
							}

							SceneInfo->bCachedRayTracingInstanceAnySegmentsDecal = InstanceMaskAndFlags.bAnySegmentsDecal;
							SceneInfo->bCachedRayTracingInstanceAllSegmentsDecal = InstanceMaskAndFlags.bAllSegmentsDecal;

							SceneInfo->bCachedRayTracingInstanceMaskAndFlagsDirty = false;
						}

						const bool bNeedMainInstance = !SceneInfo->bCachedRayTracingInstanceAllSegmentsDecal;

						// if primitive has mixed decal and non-decal segments we need to have two ray tracing instances
						// one containing non-decal segments and the other with decal segments
						// masking of segments is done using "hidden" hitgroups
						// TODO: Debug Visualization to highlight primitives using this?
						const bool bNeedDecalInstance = SceneInfo->bCachedRayTracingInstanceAnySegmentsDecal && !ShouldExcludeDecals();

						if (ShouldExcludeDecals() && SceneInfo->bCachedRayTracingInstanceAllSegmentsDecal)
						{
							continue;
						}

						int32 PrimitiveId = PersistentPrimitiveIndex.Index;
						int32 InstanceSceneDataOffset = SceneInfo->GetInstanceSceneDataOffset();

						if (Instance.Materials.Num() > 0 && Instance.Materials[0].Elements.Num() > 0 && Instance.Materials[0].Elements[0].DynamicPrimitiveData != nullptr)
						{
							check(Instance.NumTransforms == Instance.Materials[0].Elements[0].NumInstances);
							PrimitiveId = ViewDynamicPrimitiveId + Instance.Materials[0].Elements[0].DynamicPrimitiveIndex;
							InstanceSceneDataOffset = ViewInstanceSceneDataOffset + Instance.Materials[0].Elements[0].DynamicPrimitiveInstanceSceneDataOffset;
						}

						FRayTracingGeometryInstance RayTracingInstance;
						RayTracingInstance.GeometryRHI = Geometry->GetRHI();
						checkf(RayTracingInstance.GeometryRHI, TEXT("Ray tracing instance must have a valid geometry."));
						RayTracingInstance.DefaultUserData = InstanceSceneDataOffset;
						RayTracingInstance.bIncrementUserDataPerInstance = true;
						RayTracingInstance.bApplyLocalBoundsTransform = Instance.bApplyLocalBoundsTransform;
						RayTracingInstance.Mask = SceneInfo->CachedRayTracingInstance.Mask;
						RayTracingInstance.Flags = SceneInfo->CachedRayTracingInstance.Flags;
						AddDebugRayTracingInstanceFlags(RayTracingInstance.Flags);

						if (!Instance.GetPrimitiveInstanceIndices().IsEmpty())
						{
							TConstArrayView<uint32> PrimitiveInstanceIndices = Instance.GetPrimitiveInstanceIndices();

							// Convert from instance indices to InstanceSceneDataOffsets
							TArrayView<uint32> InstanceSceneDataOffsets = RayTracingScene.Allocate<uint32>(PrimitiveInstanceIndices.Num());
							for (int32 InstanceIndex = 0; InstanceIndex < PrimitiveInstanceIndices.Num(); ++InstanceIndex)
							{
								InstanceSceneDataOffsets[InstanceIndex] = SceneInfo->GetInstanceSceneDataOffset() + PrimitiveInstanceIndices[InstanceIndex];
							}

							RayTracingInstance.InstanceSceneDataOffsets = InstanceSceneDataOffsets;
							RayTracingInstance.UserData = InstanceSceneDataOffsets;
							RayTracingInstance.NumTransforms = PrimitiveInstanceIndices.Num();
						}
						else if(!Instance.GetTransforms().IsEmpty())
						{
							TConstArrayView<FMatrix> TransformsView;
							if (Instance.OwnsTransforms())
							{
								// Slow path: copy transforms to the owned storage
								checkf(Instance.InstanceTransformsView.Num() == 0, TEXT("InstanceTransformsView is expected to be empty if using InstanceTransforms"));
								TArrayView<FMatrix> SceneOwnedTransforms = RayTracingScene.Allocate<FMatrix>(Instance.InstanceTransforms.Num());
								FMemory::Memcpy(SceneOwnedTransforms.GetData(), Instance.InstanceTransforms.GetData(), Instance.InstanceTransforms.Num() * sizeof(RayTracingInstance.Transforms[0]));
								static_assert(std::is_same_v<decltype(SceneOwnedTransforms[0]), decltype(Instance.InstanceTransforms[0])>, "Unexpected transform type");

								TransformsView = SceneOwnedTransforms;
							}
							else
							{
								// Fast path: just reference persistently-allocated transforms and avoid a copy
								checkf(Instance.InstanceTransforms.Num() == 0, TEXT("InstanceTransforms is expected to be empty if using InstanceTransformsView"));
								TransformsView = Instance.InstanceTransformsView;
							}

							RayTracingInstance.NumTransforms = TransformsView.Num();
							RayTracingInstance.Transforms = TransformsView;
						}
						else
						{
							// If array of transforms was not provided, get the instance transforms from GPU Scene
							RayTracingInstance.NumTransforms = Instance.NumTransforms;
							RayTracingInstance.BaseInstanceSceneDataOffset = InstanceSceneDataOffset;
						}

						uint32 GlobalSegmentIndex = INDEX_NONE;
						if (bNeedMainInstance)
						{
							GlobalSegmentIndex = RayTracingScene.NumSegments;
							RayTracingScene.NumSegments += Instance.GetMaterials().Num();

							RayTracingInstance.InstanceContributionToHitGroupIndex = CalculateInstanceContributionToHitGroupIndex(GlobalSegmentIndex);

							RayTracingScene.AddInstance(RayTracingInstance, ERayTracingSceneLayer::Base, SceneProxy, true);
						}

						uint32 DecalGlobalSegmentIndex = INDEX_NONE;
						if (bNeedDecalInstance)
						{
							DecalGlobalSegmentIndex = RayTracingScene.NumSegments;
							RayTracingScene.NumSegments += Instance.GetMaterials().Num();

							FRayTracingGeometryInstance DecalRayTracingInstance = RayTracingInstance;
							DecalRayTracingInstance.InstanceContributionToHitGroupIndex = CalculateInstanceContributionToHitGroupIndex(DecalGlobalSegmentIndex);

							RayTracingScene.AddInstance(MoveTemp(DecalRayTracingInstance), ERayTracingSceneLayer::Decals, SceneProxy, true);
						}

						if (bParallelMeshBatchSetup)
						{
							if (NumPendingMeshBatches >= RayTracingParallelMeshBatchSize)
							{
								KickRayTracingMeshBatchTask();
							}

							if (MeshBatchTaskPage == nullptr || MeshBatchTaskPage->NumWorkItems == MaxWorkItemsPerPage)
							{
								FRayTracingMeshBatchTaskPage* NextPage = InBulkAllocator.Create<FRayTracingMeshBatchTaskPage>();
								if (MeshBatchTaskHead == nullptr)
								{
									MeshBatchTaskHead = NextPage;
								}
								if (MeshBatchTaskPage)
								{
									MeshBatchTaskPage->Next = NextPage;
								}
								MeshBatchTaskPage = NextPage;
							}

							FRayTracingMeshBatchWorkItem& WorkItem = MeshBatchTaskPage->WorkItems[MeshBatchTaskPage->NumWorkItems];
							MeshBatchTaskPage->NumWorkItems++;

							NumPendingMeshBatches += Instance.GetMaterials().Num();

							if (Instance.OwnsMaterials())
							{
								Swap(WorkItem.MeshBatchesOwned, Instance.Materials);
							}
							else
							{
								WorkItem.MeshBatchesView = Instance.MaterialsView;
							}

							WorkItem.SceneProxy = SceneProxy;
							WorkItem.RayTracingGeometry = Geometry->GetRHI();
							WorkItem.GlobalSegmentIndex = GlobalSegmentIndex;
							WorkItem.DecalGlobalSegmentIndex = DecalGlobalSegmentIndex;
						}
						else
						{
							TArrayView<const FMeshBatch> InstanceMaterials = Instance.GetMaterials();
							for (int32 SegmentIndex = 0; SegmentIndex < InstanceMaterials.Num(); SegmentIndex++)
							{
								const FMeshBatch& MeshBatch = InstanceMaterials[SegmentIndex];
								FDynamicRayTracingMeshCommandContext CommandContext(View.DynamicRayTracingMeshCommandStorage, View.VisibleRayTracingMeshCommands, Geometry->GetRHI(), SegmentIndex, GlobalSegmentIndex, DecalGlobalSegmentIndex);
								FRayTracingMeshProcessor RayTracingMeshProcessor(&CommandContext, &Scene, &View, Scene.CachedRayTracingMeshCommandsMode);
								RayTracingMeshProcessor.AddMeshBatch(MeshBatch, 1, SceneProxy);
							}
						}
					}

					if (CVarRayTracingDynamicGeometryLastRenderTimeUpdateDistance.GetValueOnRenderThread() > 0.0f)
					{
						if (FVector::Distance(SceneProxy->GetActorPosition(), View.ViewMatrices.GetViewOrigin()) < CVarRayTracingDynamicGeometryLastRenderTimeUpdateDistance.GetValueOnRenderThread())
						{
							// Update LastRenderTime for components so that visibility based ticking (like skeletal meshes) can get updated
							// We are only doing this for dynamic geometries now
							SceneInfo->LastRenderTime = CurrentWorldTime;
							SceneInfo->UpdateComponentLastRenderTime(CurrentWorldTime, /*bUpdateLastRenderTimeOnScreen=*/true);
						}
					}
				}
			}

			KickRayTracingMeshBatchTask();
		}

		// Task to iterate over static ray tracing instances, perform auto-instancing and culling.
		// This adds final instances to the ray tracing scene and must be done before FRayTracingScene::BuildInitializationData().
		struct FRayTracingSceneAddStaticInstancesTask
		{
			UE_NONCOPYABLE(FRayTracingSceneAddStaticInstancesTask)

			static ESubsequentsMode::Type GetSubsequentsMode() { return ESubsequentsMode::TrackSubsequents; }
			TStatId                       GetStatId() const { return TStatId(); }
			ENamedThreads::Type           GetDesiredThread() { return ENamedThreads::AnyThread; }

			// Inputs

			const FScene& Scene;
			const RayTracing::FSceneOptions SceneOptions;
			TArray<FRelevantPrimitive>& RelevantStaticPrimitives;
			TArray<FRelevantPrimitive>& RelevantCachedStaticPrimitives;
			TArray<FRelevantPrimitiveGatherContext>& GatherContexts;
			const FRayTracingCullingParameters& CullingParameters;
			const bool bIsPathTracing;

			const int32& NumCachedStaticInstances;
			const int32& NumCachedStaticDecalInstances;
			const int32& NumCachedStaticVisibleMeshCommands;

			// Outputs

			FRayTracingScene& RayTracingScene; // New instances are added into FRayTracingScene::Instances and FRayTracingScene::Allocator is used for temporary data
			TArray<FVisibleRayTracingMeshCommand>& VisibleRayTracingMeshCommands; // New elements are added here by this task

			FRayTracingSceneAddStaticInstancesTask(const FScene& InScene,
				const RayTracing::FSceneOptions& InSceneOptions,
				TArray<FRelevantPrimitive>& InRelevantStaticPrimitives,
				TArray<FRelevantPrimitive>& InRelevantCachedStaticPrimitives,
				TArray<FRelevantPrimitiveGatherContext>& InGatherContexts,
				const FRayTracingCullingParameters& InCullingParameters,
				const bool bInIsPathTracing,
				const int32& InNumCachedStaticInstances,
				const int32& InNumCachedStaticDecalInstances,
				const int32& InNumCachedStaticVisibleMeshCommands,
				FRayTracingScene& InRayTracingScene, TArray<FVisibleRayTracingMeshCommand>& InVisibleRayTracingMeshCommands)
				: Scene(InScene)
				, SceneOptions(InSceneOptions)
				, RelevantStaticPrimitives(InRelevantStaticPrimitives)
				, RelevantCachedStaticPrimitives(InRelevantCachedStaticPrimitives)
				, GatherContexts(InGatherContexts)
				, CullingParameters(InCullingParameters)
				, bIsPathTracing(bInIsPathTracing)
				, NumCachedStaticInstances(InNumCachedStaticInstances)
				, NumCachedStaticDecalInstances(InNumCachedStaticDecalInstances)
				, NumCachedStaticVisibleMeshCommands(InNumCachedStaticVisibleMeshCommands)
				, RayTracingScene(InRayTracingScene)
				, VisibleRayTracingMeshCommands(InVisibleRayTracingMeshCommands)
			{
				VisibleRayTracingMeshCommands.Reserve(RelevantStaticPrimitives.Num() + RelevantCachedStaticPrimitives.Num());
			}

			// TODO: Consider moving auto instance batching logic into FRayTracingScene

			struct FAutoInstanceBatch
			{
				FRayTracingScene::FInstanceHandle InstanceHandle = FRayTracingScene::INVALID_INSTANCE_HANDLE;
				FRayTracingScene::FInstanceHandle DecalInstanceHandle = FRayTracingScene::INVALID_INSTANCE_HANDLE;

				// Copies the next InstanceSceneDataOffset and user data into the current batch, returns true if arrays were re-allocated.
				bool Add(FRayTracingScene& InRayTracingScene, uint32 InInstanceSceneDataOffset)
				{
					// Adhoc TArray-like resize behavior, in lieu of support for using a custom FMemStackBase in TArray.
					// Idea for future: if batch becomes large enough, we could actually split it into multiple instances to avoid memory waste.

					const bool bNeedReallocation = Cursor == InstanceSceneDataOffsets.Num();

					if (bNeedReallocation)
					{
						int32 PrevCount = InstanceSceneDataOffsets.Num();
						int32 NextCount = FMath::Max(PrevCount * 2, 1);

						TArrayView<uint32> NewInstanceSceneDataOffsets = InRayTracingScene.Allocate<uint32>(NextCount);
						if (PrevCount)
						{
							FMemory::Memcpy(NewInstanceSceneDataOffsets.GetData(), InstanceSceneDataOffsets.GetData(), InstanceSceneDataOffsets.GetTypeSize() * InstanceSceneDataOffsets.Num());
						}
						InstanceSceneDataOffsets = NewInstanceSceneDataOffsets;
					}

					InstanceSceneDataOffsets[Cursor] = InInstanceSceneDataOffset;

					++Cursor;

					return bNeedReallocation;
				}

				bool IsValid() const
				{
					return InstanceSceneDataOffsets.Num() != 0;
				}

				TArrayView<uint32> InstanceSceneDataOffsets;
				uint32 Cursor = 0;
			};

			void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
			{
				FTaskTagScope TaskTagScope(ETaskTag::EParallelRenderingThread);

				TRACE_CPUPROFILER_EVENT_SCOPE(RayTracingSceneStaticInstanceTask);
				
				{
					TRACE_CPUPROFILER_EVENT_SCOPE(RayTracingScene_AddStaticInstances);

					const bool bAutoInstance = CVarRayTracingAutoInstance.GetValueOnRenderThread() != 0;

					// Instance batches by FRelevantPrimitive::InstancingKey()
					Experimental::TSherwoodMap<uint64, FAutoInstanceBatch> InstanceBatches;

					// scan relevant primitives computing hash data to look for duplicate instances
					for (const FRelevantPrimitive& RelevantPrimitive : RelevantStaticPrimitives)
					{
						const int32 PrimitiveIndex = RelevantPrimitive.PrimitiveIndex;
						FPrimitiveSceneInfo* SceneInfo = Scene.Primitives[PrimitiveIndex];
						FPrimitiveSceneProxy* SceneProxy = Scene.PrimitiveSceneProxies[PrimitiveIndex];
						ERayTracingPrimitiveFlags Flags = Scene.PrimitiveRayTracingFlags[PrimitiveIndex];
						const FPersistentPrimitiveIndex PersistentPrimitiveIndex = RelevantPrimitive.PersistentPrimitiveIndex;

						check(!EnumHasAnyFlags(Flags, ERayTracingPrimitiveFlags::CacheInstances));

						const int8 LODIndex = RelevantPrimitive.LODIndex;

						if (LODIndex < 0)
						{
							// TODO: Filter these primitives earlier
							continue; 
						}

						const bool bNeedMainInstance = !RelevantPrimitive.bAllSegmentsDecal;

						// if primitive has mixed decal and non-decal segments we need to have two ray tracing instances
						// one containing non-decal segments and the other with decal segments
						// masking of segments is done using "hidden" hitgroups
						// TODO: Debug Visualization to highlight primitives using this?
						const bool bNeedDecalInstance = RelevantPrimitive.bAnySegmentsDecal && !ShouldExcludeDecals();

						if (ShouldExcludeDecals() && RelevantPrimitive.bAllSegmentsDecal)
						{
							continue;
						}

						if (!SceneOptions.bTranslucentGeometry && RelevantPrimitive.bAllSegmentsTranslucent)
						{
							continue;
						}

						if (GRayTracingExcludeSky && RelevantPrimitive.bIsSky)
						{
							continue;
						}

						// location if this is a new entry
						const uint64 InstanceKey = RelevantPrimitive.InstancingKey();

						FAutoInstanceBatch DummyInstanceBatch = { };
						FAutoInstanceBatch& InstanceBatch = bAutoInstance ? InstanceBatches.FindOrAdd(InstanceKey, DummyInstanceBatch) : DummyInstanceBatch;

						if (InstanceBatch.IsValid())
						{
							// Reusing a previous entry, just append to the instance list.

							bool bReallocated = InstanceBatch.Add(RayTracingScene, SceneInfo->GetInstanceSceneDataOffset());

							if(InstanceBatch.InstanceHandle.IsValid())
							{
								FRayTracingGeometryInstance& RayTracingInstance = RayTracingScene.GetInstance(InstanceBatch.InstanceHandle);
								++RayTracingInstance.NumTransforms;
								check(RayTracingInstance.NumTransforms == InstanceBatch.Cursor); // sanity check

								if (bReallocated)
								{
									RayTracingInstance.InstanceSceneDataOffsets = InstanceBatch.InstanceSceneDataOffsets;
									RayTracingInstance.UserData = InstanceBatch.InstanceSceneDataOffsets;
								}
							}

							if (InstanceBatch.DecalInstanceHandle.IsValid())
							{
								FRayTracingGeometryInstance& RayTracingInstance = RayTracingScene.GetInstance(InstanceBatch.DecalInstanceHandle);
								++RayTracingInstance.NumTransforms;
								check(RayTracingInstance.NumTransforms == InstanceBatch.Cursor); // sanity check

								if (bReallocated)
								{
									RayTracingInstance.InstanceSceneDataOffsets = InstanceBatch.InstanceSceneDataOffsets;
									RayTracingInstance.UserData = InstanceBatch.InstanceSceneDataOffsets;
								}
							}
						}
						else
						{
							// Starting new instance batch

							InstanceBatch.Add(RayTracingScene, SceneInfo->GetInstanceSceneDataOffset());

							FRayTracingGeometryInstance RayTracingInstance;
							RayTracingInstance.GeometryRHI = RelevantPrimitive.RayTracingGeometryRHI;
							checkf(RayTracingInstance.GeometryRHI, TEXT("Ray tracing instance must have a valid geometry."));
							RayTracingInstance.InstanceSceneDataOffsets = InstanceBatch.InstanceSceneDataOffsets;
							RayTracingInstance.UserData = InstanceBatch.InstanceSceneDataOffsets;
							RayTracingInstance.NumTransforms = 1;

							RayTracingInstance.Mask = RelevantPrimitive.InstanceMask; // When no cached command is found, InstanceMask == 0 and the instance is effectively filtered out

							// Run AHS for alpha masked and meshes with only some sections casting shadows, which require per mesh section filtering in AHS
							if (RelevantPrimitive.bAllSegmentsOpaque && (RelevantPrimitive.bAllSegmentsCastShadow || !RelevantPrimitive.bAnySegmentsCastShadow))
							{
								RayTracingInstance.Flags |= ERayTracingInstanceFlags::ForceOpaque;
							}
							if (RelevantPrimitive.bTwoSided)
							{
								RayTracingInstance.Flags |= ERayTracingInstanceFlags::TriangleCullDisable;
							}
							if (RelevantPrimitive.bAllSegmentsReverseCulling)
							{
								RayTracingInstance.Flags |= ERayTracingInstanceFlags::TriangleCullReverse;
							}
							AddDebugRayTracingInstanceFlags(RayTracingInstance.Flags);

							InstanceBatch.InstanceHandle = FRayTracingScene::INVALID_INSTANCE_HANDLE;
							uint32 GlobalSegmentIndex = INDEX_NONE;

							if (bNeedMainInstance)
							{
								GlobalSegmentIndex = RayTracingScene.NumSegments;
								RayTracingScene.NumSegments += RayTracingInstance.GeometryRHI->GetNumSegments();

								RayTracingInstance.InstanceContributionToHitGroupIndex = CalculateInstanceContributionToHitGroupIndex(GlobalSegmentIndex);

								InstanceBatch.InstanceHandle = RayTracingScene.AddInstance(RayTracingInstance, ERayTracingSceneLayer::Base, SceneProxy, false);
							}

							InstanceBatch.DecalInstanceHandle = FRayTracingScene::INVALID_INSTANCE_HANDLE;
							uint32 DecalGlobalSegmentIndex = INDEX_NONE;

							if (bNeedDecalInstance)
							{
								DecalGlobalSegmentIndex = RayTracingScene.NumSegments;
								RayTracingScene.NumSegments += RayTracingInstance.GeometryRHI->GetNumSegments();

								FRayTracingGeometryInstance DecalRayTracingInstance = RayTracingInstance;
								DecalRayTracingInstance.InstanceContributionToHitGroupIndex = CalculateInstanceContributionToHitGroupIndex(DecalGlobalSegmentIndex);

								InstanceBatch.DecalInstanceHandle = RayTracingScene.AddInstance(MoveTemp(DecalRayTracingInstance), ERayTracingSceneLayer::Decals, SceneProxy, false);
							}

							for (int32 CommandIndex : RelevantPrimitive.CachedRayTracingMeshCommandIndices)
							{
								if (CommandIndex >= 0)
								{
									const FRayTracingMeshCommand& MeshCommand = Scene.CachedRayTracingMeshCommands[CommandIndex];

									if (bNeedMainInstance)
									{
										const bool bHidden = MeshCommand.bDecal;
										FVisibleRayTracingMeshCommand NewVisibleMeshCommand(&MeshCommand, RelevantPrimitive.RayTracingGeometryRHI, GlobalSegmentIndex + MeshCommand.GeometrySegmentIndex, bHidden);
										VisibleRayTracingMeshCommands.Add(NewVisibleMeshCommand);
									}

									if (bNeedDecalInstance)
									{
										const bool bHidden = !MeshCommand.bDecal;
										FVisibleRayTracingMeshCommand NewVisibleMeshCommand(&MeshCommand, RelevantPrimitive.RayTracingGeometryRHI, DecalGlobalSegmentIndex + MeshCommand.GeometrySegmentIndex, bHidden);
										VisibleRayTracingMeshCommands.Add(NewVisibleMeshCommand);
									}
								}
								else
								{
									// CommandIndex == -1 indicates that the mesh batch has been filtered by FRayTracingMeshProcessor (like the shadow depth pass batch)
									// Do nothing in this case
								}
							}
						}
					}
				}

				{
					TRACE_CPUPROFILER_EVENT_SCOPE(RayTracingScene_AddCachedStaticInstances);
					
					const FRayTracingScene::FInstanceRange CachedStaticInstanceRange = RayTracingScene.AllocateInstanceRangeUninitialized(NumCachedStaticInstances, ERayTracingSceneLayer::Base);
					const FRayTracingScene::FInstanceRange CachedStaticDecalInstanceRange = RayTracingScene.AllocateInstanceRangeUninitialized(NumCachedStaticDecalInstances, ERayTracingSceneLayer::Decals);
					const uint32 BaseCachedVisibleMeshCommandsIndex = VisibleRayTracingMeshCommands.AddUninitialized(NumCachedStaticVisibleMeshCommands);
					const uint32 BaseCachedGlobalSegmentIndex = RayTracingScene.NumSegments;
					RayTracingScene.NumSegments += NumCachedStaticVisibleMeshCommands;

					const int32 MinBatchSize = 128;
					ParallelFor(
						TEXT("RayTracingScene_AddCachedStaticInstances_ParallelFor"),
						RelevantCachedStaticPrimitives.Num(),
						MinBatchSize,
						[this, CachedStaticInstanceRange, CachedStaticDecalInstanceRange, BaseCachedVisibleMeshCommandsIndex, BaseCachedGlobalSegmentIndex](int32 Index)
					{
						const FRelevantPrimitive& RelevantPrimitive = RelevantCachedStaticPrimitives[Index];
						const int32 PrimitiveIndex = RelevantPrimitive.PrimitiveIndex;
						FPrimitiveSceneInfo* SceneInfo = Scene.Primitives[PrimitiveIndex];
						FPrimitiveSceneProxy* SceneProxy = Scene.PrimitiveSceneProxies[PrimitiveIndex];
						ERayTracingPrimitiveFlags Flags = Scene.PrimitiveRayTracingFlags[PrimitiveIndex];
						const FPersistentPrimitiveIndex PersistentPrimitiveIndex = RelevantPrimitive.PersistentPrimitiveIndex;

						check(EnumHasAnyFlags(Flags, ERayTracingPrimitiveFlags::CacheInstances));

						const bool bUsingNaniteRayTracing = (Nanite::GetRayTracingMode() != Nanite::ERayTracingMode::Fallback) && SceneProxy->IsNaniteMesh();

						if (bUsingNaniteRayTracing)
						{
							check(RelevantPrimitive.CachedRayTracingInstance->GeometryRHI != nullptr);
						}

						const bool bNeedMainInstance = !RelevantPrimitive.bAllSegmentsDecal;

						// if primitive has mixed decal and non-decal segments we need to have two ray tracing instances
						// one containing non-decal segments and the other with decal segments
						// masking of segments is done using "hidden" hitgroups
						// TODO: Debug Visualization to highlight primitives using this?
						const bool bNeedDecalInstance = RelevantPrimitive.bAnySegmentsDecal && !RelevantPrimitive.bAllSegmentsDecal && !ShouldExcludeDecals();

						check(!ShouldExcludeDecals() || !RelevantPrimitive.bAllSegmentsDecal);
						
						const int32 GlobalSegmentIndex = BaseCachedGlobalSegmentIndex + GatherContexts[RelevantPrimitive.ContextIndex].VisibleMeshCommandOffset + RelevantPrimitive.RelativeVisibleMeshCommandOffset;
						const int32 MainGlobalSegmentIndex = GlobalSegmentIndex;
						const int32 DecalGlobalSegmentIndex = GlobalSegmentIndex + (bNeedMainInstance ? RelevantPrimitive.CachedRayTracingMeshCommandIndices.Num() : 0);

						check(RelevantPrimitive.CachedRayTracingInstance);

						if (bNeedMainInstance)
						{
							const int32 InstanceIndexInRange = GatherContexts[RelevantPrimitive.ContextIndex].InstanceOffset + RelevantPrimitive.RelativeInstanceOffset;

							FRayTracingGeometryInstance RayTracingInstance = *RelevantPrimitive.CachedRayTracingInstance;
							RayTracingInstance.InstanceContributionToHitGroupIndex = CalculateInstanceContributionToHitGroupIndex(MainGlobalSegmentIndex);
							AddDebugRayTracingInstanceFlags(RayTracingInstance.Flags);

							RayTracingScene.SetInstance(CachedStaticInstanceRange, InstanceIndexInRange, MoveTemp(RayTracingInstance), SceneProxy, false);
						}

						if (bNeedDecalInstance)
						{
							const int32 DecalInstanceIndexInRange = GatherContexts[RelevantPrimitive.ContextIndex].DecalInstanceOffset + RelevantPrimitive.RelativeDecalInstanceOffset;

							FRayTracingGeometryInstance DecalRayTracingInstance = *RelevantPrimitive.CachedRayTracingInstance;
							DecalRayTracingInstance.InstanceContributionToHitGroupIndex = CalculateInstanceContributionToHitGroupIndex(DecalGlobalSegmentIndex);
							AddDebugRayTracingInstanceFlags(DecalRayTracingInstance.Flags);

							RayTracingScene.SetInstance(CachedStaticDecalInstanceRange, DecalInstanceIndexInRange, MoveTemp(DecalRayTracingInstance), SceneProxy, false);
						}

						const int32 VisibleMeshCommandOffset = BaseCachedVisibleMeshCommandsIndex + GatherContexts[RelevantPrimitive.ContextIndex].VisibleMeshCommandOffset + RelevantPrimitive.RelativeVisibleMeshCommandOffset;

						int32 MainCommandOffset = VisibleMeshCommandOffset;
						int32 DecalCommandOffset = VisibleMeshCommandOffset + (bNeedMainInstance ? RelevantPrimitive.CachedRayTracingMeshCommandIndices.Num() : 0);

						for (int32 CommandIndex : RelevantPrimitive.CachedRayTracingMeshCommandIndices)
						{
							const FRayTracingMeshCommand& MeshCommand = Scene.CachedRayTracingMeshCommands[CommandIndex];

							if (bNeedMainInstance)
							{
								const bool bHidden = MeshCommand.bDecal;
								VisibleRayTracingMeshCommands[MainCommandOffset] = FVisibleRayTracingMeshCommand(&MeshCommand, RelevantPrimitive.CachedRayTracingInstance->GeometryRHI, MainGlobalSegmentIndex + MeshCommand.GeometrySegmentIndex, bHidden);
								++MainCommandOffset;
							}

							if (bNeedDecalInstance)
							{
								const bool bHidden = !MeshCommand.bDecal;
								VisibleRayTracingMeshCommands[DecalCommandOffset] = FVisibleRayTracingMeshCommand(&MeshCommand, RelevantPrimitive.CachedRayTracingInstance->GeometryRHI, DecalGlobalSegmentIndex + MeshCommand.GeometrySegmentIndex, bHidden);
								++DecalCommandOffset;
							}
						}
					});
				}
			}
		};

		FGraphEventArray AddStaticInstancesTaskPrerequisites;
		AddStaticInstancesTaskPrerequisites.Add(RelevantPrimitiveList.StaticPrimitiveLODTask);

		FGraphEventRef AddStaticInstancesTask = TGraphTask<FRayTracingSceneAddStaticInstancesTask>::CreateTask(&AddStaticInstancesTaskPrerequisites).ConstructAndDispatchWhenReady(
			// inputs
			Scene,
			SceneOptions,
			RelevantPrimitiveList.StaticPrimitives,
			RelevantPrimitiveList.CachedStaticPrimitives,
			RelevantPrimitiveList.GatherContexts,
			View.RayTracingCullingParameters,
			bool(View.Family->EngineShowFlags.PathTracing),
			RelevantPrimitiveList.NumCachedStaticInstances,
			RelevantPrimitiveList.NumCachedStaticDecalInstances,
			RelevantPrimitiveList.NumCachedStaticVisibleMeshCommands,
			// outputs
			RayTracingScene,
			View.VisibleRayTracingMeshCommands
		);

		// Scene init task can run only when all pre-init tasks are complete (including culling tasks that are spawned while adding instances)
		View.RayTracingSceneInitTask = FFunctionGraphTask::CreateAndDispatchWhenReady(
			[&View, &RayTracingScene]()
			{
				FTaskTagScope TaskTagScope(ETaskTag::EParallelRenderingThread);

				TRACE_CPUPROFILER_EVENT_SCOPE(RayTracingSceneInitTask);
				RayTracingScene.BuildInitializationData();
			},
			TStatId(), AddStaticInstancesTask, ENamedThreads::AnyThread);

		return true;
	}

	bool ShouldExcludeDecals()
	{
		return GRayTracingExcludeDecals != 0;
	}
}

static_assert(std::is_trivially_destructible_v<RayTracing::FRelevantPrimitive>, "FRelevantPrimitive must be trivially destructible");
template <> struct TIsPODType<RayTracing::FRelevantPrimitive> { enum { Value = true }; }; // Necessary to use TChunkedArray::CopyToLinearArray

#endif //RHI_RAYTRACING
