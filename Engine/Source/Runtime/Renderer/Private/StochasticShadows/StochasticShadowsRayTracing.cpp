// Copyright Epic Games, Inc. All Rights Reserved.

#include "StochasticShadows.h"
#include "StochasticShadowsInternal.h"
#include "Lumen/LumenTracingUtils.h"
#include "Lumen/LumenHardwareRayTracingCommon.h"
#include "BasePassRendering.h"

static TAutoConsoleVariable<int32> CVarStochasticShadowsScreenTraces(
	TEXT("r.StochasticShadows.ScreenTraces"),
	0,
	TEXT("Whether to use screen space tracing for shadow rays."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarStochasticShadowsScreenTracesMaxIterations(
	TEXT("r.StochasticShadows.ScreenTraces.MaxIterations"),
	50,
	TEXT("Max iterations for HZB tracing."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarStochasticShadowsScreenTracesMinimumOccupancy(
	TEXT("r.StochasticShadows.ScreenTraces.MinimumOccupancy"),
	0,
	TEXT("Minimum number of threads still tracing before aborting the trace. Can be used for scalability to abandon traces that have a disproportionate cost."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> CVarStochasticShadowsScreenTraceRelativeDepthThreshold(
	TEXT("r.StochasticShadows.ScreenTraces.RelativeDepthThickness"),
	0.005f,
	TEXT("Determines depth thickness of objects hit by HZB tracing, as a relative depth threshold."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarStochasticShadowsWorldSpaceTraces(
	TEXT("r.StochasticShadows.WorldSpaceTraces"),
	1,
	TEXT("Whether to trace world space shadow rays for samples. Useful for debugging."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarStochasticShadowsHardwareRayTracing(
	TEXT("r.StochasticShadows.HardwareRayTracing"),
	1,
	TEXT("Whether to use hardware ray tracing for shadow rays."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarStochasticShadowsHardwareRayTracingInline(
	TEXT("r.StochasticShadows.HardwareRayTracing.Inline"),
	1,
	TEXT("Uses hardware inline ray tracing for ray traced lighting, when available."),
	ECVF_RenderThreadSafe | ECVF_Scalability
);

static TAutoConsoleVariable<float> CVarStochasticShadowsHardwareRayTracingBias(
	TEXT("r.StochasticShadows.HardwareRayTracing.Bias"),
	1.0f,
	TEXT("Constant bias for hardware ray traced shadow rays."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> CVarStochasticShadowsHardwareRayTracingNormalBias(
	TEXT("r.StochasticShadows.HardwareRayTracing.NormalBias"),
	0.1f,
	TEXT("Normal bias for hardware ray traced shadow rays."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarStochasticShadowsHardwareRayTracingMaxIterations(
	TEXT("r.StochasticShadows.HardwareRayTracing.MaxIterations"),
	8192,
	TEXT("Limit number of ray tracing traversal iterations on supported platfoms. Improves performance, but may add over-occlusion."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarStochasticShadowsHairVoxelTraces(
	TEXT("r.StochasticShadows.HairVoxelTraces"),
	1,
	TEXT("Whether to trace hair voxels."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

namespace StochasticShadows
{
	bool UseHardwareRayTracing()
	{
		#if RHI_RAYTRACING
		{
			return IsRayTracingEnabled() 
				&& CVarStochasticShadowsHardwareRayTracing.GetValueOnRenderThread() != 0;
		}
		#else
		{
			return false;
		}
		#endif
	}

	bool UseInlineHardwareRayTracing()
	{
		#if RHI_RAYTRACING
		{
			return UseHardwareRayTracing()
				&& GRHISupportsInlineRayTracing
				&& CVarStochasticShadowsHardwareRayTracingInline.GetValueOnRenderThread() != 0;
		}
		#else
		{
			return false;
		}
		#endif
	}

	bool IsUsingClosestHZB()
	{
		return IsEnabled() && CVarStochasticShadowsScreenTraces.GetValueOnRenderThread() != 0;
	}

	bool IsUsingGlobalSDF()
	{
		return IsEnabled() && CVarStochasticShadowsWorldSpaceTraces.GetValueOnRenderThread() != 0 && !UseHardwareRayTracing();
	}

	BEGIN_SHADER_PARAMETER_STRUCT(FCompactedTraceParameters, )
		RDG_BUFFER_ACCESS(IndirectArgs, ERHIAccess::IndirectArgs)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, CompactedTraceTexelData)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, CompactedTraceTexelAllocator)
	END_SHADER_PARAMETER_STRUCT()

	enum class ECompactedTraceIndirectArgs
	{
		NumTracesDiv64 = 0 * sizeof(FRHIDispatchIndirectParameters),
		NumTracesDiv32 = 1 * sizeof(FRHIDispatchIndirectParameters),
		NumTraces = 2 * sizeof(FRHIDispatchIndirectParameters),
		MAX = 3
	};

	BEGIN_SHADER_PARAMETER_STRUCT(FHairParameters, )
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FHairStrandsViewUniformParameters, HairStrands)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FVirtualVoxelParameters, VirtualVoxel)
	END_SHADER_PARAMETER_STRUCT()

	FCompactedTraceParameters CompactStochasticShadowsTraces(
		const FViewInfo& View,
		FRDGBuilder& GraphBuilder,
		const FIntPoint SampleBufferSize,
		FRDGTextureRef LightSamples,
		const FStochasticShadowsParameters& StochasticShadowsParameters);
};

class FCompactLightSampleTracesCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FCompactLightSampleTracesCS)
	SHADER_USE_PARAMETER_STRUCT(FCompactLightSampleTracesCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FStochasticShadowsParameters, StochasticShadowsParameters)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, RWCompactedTraceTexelData)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, RWCompactedTraceTexelAllocator)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<uint>, LightSamples)
	END_SHADER_PARAMETER_STRUCT()

	static int32 GetGroupSize()
	{
		return 16;
	}

	class FWaveOps : SHADER_PERMUTATION_BOOL("WAVE_OPS");
	using FPermutationDomain = TShaderPermutationDomain<FWaveOps>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return StochasticShadows::ShouldCompileShaders(Parameters);
	}

	FORCENOINLINE static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());

		FPermutationDomain PermutationVector(Parameters.PermutationId);
		if (PermutationVector.Get<FWaveOps>())
		{
			OutEnvironment.CompilerFlags.Add(CFLAG_WaveOperations);
		}
	}
};

IMPLEMENT_GLOBAL_SHADER(FCompactLightSampleTracesCS, "/Engine/Private/StochasticShadows/StochasticShadowsTracing.usf", "CompactLightSampleTracesCS", SF_Compute);

class FInitCompactedTraceTexelIndirectArgsCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FInitCompactedTraceTexelIndirectArgsCS)
	SHADER_USE_PARAMETER_STRUCT(FInitCompactedTraceTexelIndirectArgsCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FStochasticShadowsParameters, StochasticShadowsParameters)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWIndirectArgs)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, CompactedTraceTexelAllocator)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return StochasticShadows::ShouldCompileShaders(Parameters);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());
	}

	static int32 GetGroupSize()
	{
		return 64;
	}
};

IMPLEMENT_GLOBAL_SHADER(FInitCompactedTraceTexelIndirectArgsCS, "/Engine/Private/StochasticShadows/StochasticShadowsTracing.usf", "InitCompactedTraceTexelIndirectArgsCS", SF_Compute);

#if RHI_RAYTRACING

class FHardwareRayTraceLightSamples : public FLumenHardwareRayTracingShaderBase
{
	DECLARE_LUMEN_RAYTRACING_SHADER(FHardwareRayTraceLightSamples, Lumen::ERayTracingShaderDispatchSize::DispatchSize1D)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(StochasticShadows::FCompactedTraceParameters, CompactedTraceParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(FStochasticShadowsParameters, StochasticShadowsParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(StochasticShadows::FHairParameters, HairParameters)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<uint>, RWLightSamples)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float>, LightSampleRayDistance)
		SHADER_PARAMETER(float, RayTracingBias)
		SHADER_PARAMETER(float, RayTracingNormalBias)
		// Ray Tracing
		SHADER_PARAMETER(uint32, MaxTraversalIterations)
		SHADER_PARAMETER_RDG_BUFFER_SRV(RaytracingAccelerationStructure, TLAS)
		SHADER_PARAMETER_SRV(StructuredBuffer, RayTracingSceneMetadata)
		// Inline Ray Tracing
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<Lumen::FHitGroupRootConstants>, HitGroupData)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FLumenHardwareRayTracingUniformBufferParameters, LumenHardwareRayTracingUniformBuffer)
	END_SHADER_PARAMETER_STRUCT()

	class FHairVoxelTraces : SHADER_PERMUTATION_BOOL("HAIR_VOXEL_TRACES");
	class FDebugMode : SHADER_PERMUTATION_BOOL("DEBUG_MODE");
	using FPermutationDomain = TShaderPermutationDomain<FHairVoxelTraces, FDebugMode>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters, Lumen::ERayTracingShaderDispatchType ShaderDispatchType)
	{
		return StochasticShadows::ShouldCompileShaders(Parameters)  
			&& FLumenHardwareRayTracingShaderBase::ShouldCompilePermutation(Parameters, ShaderDispatchType);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, Lumen::ERayTracingShaderDispatchType ShaderDispatchType, FShaderCompilerEnvironment& OutEnvironment)
	{
		FLumenHardwareRayTracingShaderBase::ModifyCompilationEnvironment(Parameters, ShaderDispatchType, Lumen::ESurfaceCacheSampling::AlwaysResidentPagesWithoutFeedback, OutEnvironment);
		StochasticShadows::ModifyCompilationEnvironment(Parameters.Platform, OutEnvironment);
	}

	static ERayTracingPayloadType GetRayTracingPayloadType(const int32 PermutationId)
	{
		return ERayTracingPayloadType::LumenMinimal;
	}
};

IMPLEMENT_LUMEN_RAYGEN_AND_COMPUTE_RAYTRACING_SHADERS(FHardwareRayTraceLightSamples)

IMPLEMENT_GLOBAL_SHADER(FHardwareRayTraceLightSamplesCS, "/Engine/Private/StochasticShadows/StochasticShadowsHardwareRayTracing.usf", "HardwareRayTraceLightSamplesCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FHardwareRayTraceLightSamplesRGS, "/Engine/Private/StochasticShadows/StochasticShadowsHardwareRayTracing.usf", "HardwareRayTraceLightSamplesRGS", SF_RayGen);

#endif // RHI_RAYTRACING

class FSoftwareRayTraceLightSamplesCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FSoftwareRayTraceLightSamplesCS)
	SHADER_USE_PARAMETER_STRUCT(FSoftwareRayTraceLightSamplesCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(StochasticShadows::FCompactedTraceParameters, CompactedTraceParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(FStochasticShadowsParameters, StochasticShadowsParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(StochasticShadows::FHairParameters, HairParameters)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<uint>, RWLightSamples)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float>, LightSampleRayDistance)
	END_SHADER_PARAMETER_STRUCT()

	static int32 GetGroupSize()
	{
		return 64;
	}

	class FHairVoxelTraces : SHADER_PERMUTATION_BOOL("HAIR_VOXEL_TRACES");
	class FDebugMode : SHADER_PERMUTATION_BOOL("DEBUG_MODE");
	using FPermutationDomain = TShaderPermutationDomain<FHairVoxelTraces, FDebugMode>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return StochasticShadows::ShouldCompileShaders(Parameters);
	}

	FORCENOINLINE static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		StochasticShadows::ModifyCompilationEnvironment(Parameters.Platform, OutEnvironment);
		OutEnvironment.CompilerFlags.Add(CFLAG_Wave32);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());

		// GPU Scene definitions
		OutEnvironment.SetDefine(TEXT("VF_SUPPORTS_PRIMITIVE_SCENE_DATA"), 1);
	}
};

IMPLEMENT_GLOBAL_SHADER(FSoftwareRayTraceLightSamplesCS, "/Engine/Private/StochasticShadows/StochasticShadowsTracing.usf", "SoftwareRayTraceLightSamplesCS", SF_Compute);

class FScreenSpaceRayTraceLightSamplesCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FScreenSpaceRayTraceLightSamplesCS)
	SHADER_USE_PARAMETER_STRUCT(FScreenSpaceRayTraceLightSamplesCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(StochasticShadows::FCompactedTraceParameters, CompactedTraceParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(FStochasticShadowsParameters, StochasticShadowsParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(StochasticShadows::FHairParameters, HairParameters)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<uint>, RWLightSamples)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float>, RWLightSampleRayDistance)
		SHADER_PARAMETER_STRUCT_INCLUDE(FLumenHZBScreenTraceParameters, HZBScreenTraceParameters)
		SHADER_PARAMETER(float, MaxHierarchicalScreenTraceIterations)
		SHADER_PARAMETER(float, RelativeDepthThickness)
		SHADER_PARAMETER(float, HistoryDepthTestRelativeThickness)
		SHADER_PARAMETER(uint32, MinimumTracingThreadOccupancy)
	END_SHADER_PARAMETER_STRUCT()

	static int32 GetGroupSize()
	{
		return 64;
	}

	class FHairVoxelTraces : SHADER_PERMUTATION_BOOL("HAIR_VOXEL_TRACES");
	class FDebugMode : SHADER_PERMUTATION_BOOL("DEBUG_MODE");
	using FPermutationDomain = TShaderPermutationDomain<FHairVoxelTraces, FDebugMode>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return StochasticShadows::ShouldCompileShaders(Parameters);
	}

	FORCENOINLINE static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		StochasticShadows::ModifyCompilationEnvironment(Parameters.Platform, OutEnvironment);
		OutEnvironment.CompilerFlags.Add(CFLAG_Wave32);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());
	}
};

IMPLEMENT_GLOBAL_SHADER(FScreenSpaceRayTraceLightSamplesCS, "/Engine/Private/StochasticShadows/StochasticShadowsTracing.usf", "ScreenSpaceRayTraceLightSamplesCS", SF_Compute);

#if RHI_RAYTRACING
void FDeferredShadingSceneRenderer::PrepareStochasticShadowsLumenMaterial(const FViewInfo& View, TArray<FRHIRayTracingShader*>& OutRayGenShaders)
{
	if (StochasticShadows::IsEnabled() && StochasticShadows::UseHardwareRayTracing())
	{
		for (int32 HairVoxelTraces = 0; HairVoxelTraces < 2; ++HairVoxelTraces)
		{
			FHardwareRayTraceLightSamplesRGS::FPermutationDomain PermutationVector;
			PermutationVector.Set<FHardwareRayTraceLightSamplesRGS::FHairVoxelTraces>(HairVoxelTraces != 0);
			PermutationVector.Set<FHardwareRayTraceLightSamplesRGS::FDebugMode>(StochasticShadows::GetDebugMode() != 0);
			TShaderRef<FHardwareRayTraceLightSamplesRGS> RayGenerationShader = View.ShaderMap->GetShader<FHardwareRayTraceLightSamplesRGS>(PermutationVector);
			OutRayGenShaders.Add(RayGenerationShader.GetRayTracingShader());
		}
	}
}

namespace StochasticShadows
{
	void SetHardwareRayTracingPassParameters(
		const FViewInfo& View,
		FRDGBuilder& GraphBuilder,
		const FCompactedTraceParameters& CompactedTraceParameters,
		const FStochasticShadowsParameters& StochasticShadowsParameters,
		FRDGTextureRef LightSamples,
		FRDGTextureRef LightSampleRayDistance,
		FHardwareRayTraceLightSamples::FParameters* PassParameters);
};

void StochasticShadows::SetHardwareRayTracingPassParameters(
	const FViewInfo& View,
	FRDGBuilder& GraphBuilder,
	const FCompactedTraceParameters& CompactedTraceParameters,
	const FStochasticShadowsParameters& StochasticShadowsParameters,
	FRDGTextureRef LightSamples,
	FRDGTextureRef LightSampleRayDistance,
	FHardwareRayTraceLightSamples::FParameters* PassParameters)
{
	PassParameters->CompactedTraceParameters = CompactedTraceParameters;
	PassParameters->StochasticShadowsParameters = StochasticShadowsParameters;
	PassParameters->RWLightSamples = GraphBuilder.CreateUAV(LightSamples);
	PassParameters->LightSampleRayDistance = LightSampleRayDistance;
	PassParameters->RayTracingBias = CVarStochasticShadowsHardwareRayTracingBias.GetValueOnRenderThread();
	PassParameters->RayTracingNormalBias = CVarStochasticShadowsHardwareRayTracingNormalBias.GetValueOnRenderThread();

	checkf(View.HasRayTracingScene(), TEXT("TLAS does not exist. Verify that the current pass is represented in Lumen::AnyLumenHardwareRayTracingPassEnabled()."));
	PassParameters->TLAS = View.GetRayTracingSceneLayerViewChecked(ERayTracingSceneLayer::Base);
	PassParameters->MaxTraversalIterations = FMath::Max(CVarStochasticShadowsHardwareRayTracingMaxIterations.GetValueOnRenderThread(), 1);

	// Inline
	PassParameters->HitGroupData = View.GetPrimaryView()->LumenHardwareRayTracingHitDataBuffer ? GraphBuilder.CreateSRV(View.GetPrimaryView()->LumenHardwareRayTracingHitDataBuffer) : nullptr;
	PassParameters->LumenHardwareRayTracingUniformBuffer = View.GetPrimaryView()->LumenHardwareRayTracingUniformBuffer ? View.GetPrimaryView()->LumenHardwareRayTracingUniformBuffer : nullptr;
	checkf(View.RayTracingSceneInitTask == nullptr, TEXT("RayTracingSceneInitTask must be completed before creating SRV for RayTracingSceneMetadata."));
	PassParameters->RayTracingSceneMetadata = View.GetRayTracingSceneChecked()->GetOrCreateMetadataBufferSRV(GraphBuilder.RHICmdList);
}

#endif

StochasticShadows::FCompactedTraceParameters StochasticShadows::CompactStochasticShadowsTraces(
	const FViewInfo& View,
	FRDGBuilder& GraphBuilder,
	const FIntPoint SampleBufferSize,
	FRDGTextureRef LightSamples,
	const FStochasticShadowsParameters& StochasticShadowsParameters)
{
	FRDGBufferRef CompactedTraceTexelData = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), SampleBufferSize.X * SampleBufferSize.Y),
		TEXT("StochasticShadowsParameters.CompactedTraceTexelData"));

	FRDGBufferRef CompactedTraceTexelAllocator = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), 1),
		TEXT("StochasticShadowsParameters.CompactedTraceTexelAllocator"));

	FRDGBufferRef CompactedTraceTexelIndirectArgs = GraphBuilder.CreateBuffer(
		FRDGBufferDesc::CreateIndirectDesc<FRHIDispatchIndirectParameters>((int32)StochasticShadows::ECompactedTraceIndirectArgs::MAX),
		TEXT("StochasticShadows.CompactedTraceTexelIndirectArgs"));

	AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(CompactedTraceTexelAllocator), 0);

	// Compact light sample traces before tracing
	{
		FCompactLightSampleTracesCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FCompactLightSampleTracesCS::FParameters>();
		PassParameters->RWCompactedTraceTexelData = GraphBuilder.CreateUAV(CompactedTraceTexelData);
		PassParameters->RWCompactedTraceTexelAllocator = GraphBuilder.CreateUAV(CompactedTraceTexelAllocator);
		PassParameters->StochasticShadowsParameters = StochasticShadowsParameters;
		PassParameters->LightSamples = LightSamples;

		const bool bWaveOps = StochasticShadows::UseWaveOps(View.GetShaderPlatform())
			&& GRHIMinimumWaveSize <= 32
			&& GRHIMaximumWaveSize >= 32;

		FCompactLightSampleTracesCS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FCompactLightSampleTracesCS::FWaveOps>(bWaveOps);
		auto ComputeShader = View.ShaderMap->GetShader<FCompactLightSampleTracesCS>(PermutationVector);

		const FIntVector GroupCount = FComputeShaderUtils::GetGroupCount(StochasticShadowsParameters.SampleViewSize, FCompactLightSampleTracesCS::GetGroupSize());

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("CompactLightSampleTraces"),
			ComputeShader,
			PassParameters,
			GroupCount);
	}

	// Setup indirect args for tracing
	{
		FInitCompactedTraceTexelIndirectArgsCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FInitCompactedTraceTexelIndirectArgsCS::FParameters>();
		PassParameters->StochasticShadowsParameters = StochasticShadowsParameters;
		PassParameters->RWIndirectArgs = GraphBuilder.CreateUAV(CompactedTraceTexelIndirectArgs);
		PassParameters->CompactedTraceTexelAllocator = GraphBuilder.CreateSRV(CompactedTraceTexelAllocator);

		auto ComputeShader = View.ShaderMap->GetShader<FInitCompactedTraceTexelIndirectArgsCS>();

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("InitCompactedTraceTexelIndirectArgs"),
			ComputeShader,
			PassParameters,
			FIntVector(1, 1, 1));
	}

	FCompactedTraceParameters Parameters;
	Parameters.CompactedTraceTexelAllocator = GraphBuilder.CreateSRV(CompactedTraceTexelAllocator);
	Parameters.CompactedTraceTexelData = GraphBuilder.CreateSRV(CompactedTraceTexelData);
	Parameters.IndirectArgs = CompactedTraceTexelIndirectArgs;
	return Parameters;
}

/**
 * Ray trace light samples using a variety of tracing methods depending on the feature configuration.
 */
void StochasticShadows::RayTraceLightSamples(
	const FViewInfo& View,
	FRDGBuilder& GraphBuilder,
	const FSceneTextures& SceneTextures,
	const FIntPoint SampleBufferSize,
	FRDGTextureRef LightSamples,
	FRDGTextureRef LightSampleRayDistance,
	const FStochasticShadowsParameters& StochasticShadowsParameters)
{
	const bool bDebug = StochasticShadows::GetDebugMode() != 0;

	if (CVarStochasticShadowsScreenTraces.GetValueOnRenderThread() != 0)
	{
		FCompactedTraceParameters CompactedTraceParameters = CompactStochasticShadowsTraces(
			View,
			GraphBuilder,
			SampleBufferSize,
			LightSamples,
			StochasticShadowsParameters);

		FScreenSpaceRayTraceLightSamplesCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FScreenSpaceRayTraceLightSamplesCS::FParameters>();
		PassParameters->CompactedTraceParameters = CompactedTraceParameters;
		PassParameters->StochasticShadowsParameters = StochasticShadowsParameters;
		PassParameters->RWLightSamples = GraphBuilder.CreateUAV(LightSamples);
		PassParameters->RWLightSampleRayDistance = GraphBuilder.CreateUAV(LightSampleRayDistance);
		PassParameters->HZBScreenTraceParameters = SetupHZBScreenTraceParameters(GraphBuilder, View, SceneTextures, /*bBindLumenHistory*/ false);
		PassParameters->MaxHierarchicalScreenTraceIterations = CVarStochasticShadowsScreenTracesMaxIterations.GetValueOnRenderThread();
		PassParameters->RelativeDepthThickness = CVarStochasticShadowsScreenTraceRelativeDepthThreshold.GetValueOnRenderThread() * View.ViewMatrices.GetPerProjectionDepthThicknessScale();
		PassParameters->HistoryDepthTestRelativeThickness = 0.0f;
		PassParameters->MinimumTracingThreadOccupancy = CVarStochasticShadowsScreenTracesMinimumOccupancy.GetValueOnRenderThread();

		FScreenSpaceRayTraceLightSamplesCS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FScreenSpaceRayTraceLightSamplesCS::FDebugMode>(bDebug);
		auto ComputeShader = View.ShaderMap->GetShader<FScreenSpaceRayTraceLightSamplesCS>(PermutationVector);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("ScreenSpaceRayTraceLightSamples"),
			ComputeShader,
			PassParameters,
			CompactedTraceParameters.IndirectArgs,
			(int32)StochasticShadows::ECompactedTraceIndirectArgs::NumTracesDiv64);
	}
	else
	{
		AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(LightSampleRayDistance), 0.0f);
	}

	const bool bHairVoxelTraces = HairStrands::HasViewHairStrandsData(View)
		&& HairStrands::HasViewHairStrandsVoxelData(View)
		&& CVarStochasticShadowsHairVoxelTraces.GetValueOnRenderThread() != 0;

	StochasticShadows::FHairParameters HairParameters;
	if (bHairVoxelTraces)
	{
		HairParameters.HairStrands = HairStrands::BindHairStrandsViewUniformParameters(View);
		HairParameters.VirtualVoxel = HairStrands::BindHairStrandsVoxelUniformParameters(View);
	}

	if (CVarStochasticShadowsWorldSpaceTraces.GetValueOnRenderThread() != 0)
	{
		FCompactedTraceParameters CompactedTraceParameters = CompactStochasticShadowsTraces(
			View,
			GraphBuilder,
			SampleBufferSize,
			LightSamples,
			StochasticShadowsParameters);

		if (StochasticShadows::UseHardwareRayTracing())
		{
			#if RHI_RAYTRACING
			if (StochasticShadows::UseInlineHardwareRayTracing())
			{
				FHardwareRayTraceLightSamplesCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FHardwareRayTraceLightSamplesCS::FParameters>();
				StochasticShadows::SetHardwareRayTracingPassParameters(
					View,
					GraphBuilder,
					CompactedTraceParameters,
					StochasticShadowsParameters,
					LightSamples,
					LightSampleRayDistance,
					PassParameters);
				PassParameters->HairParameters = HairParameters;

				FHardwareRayTraceLightSamplesCS::FPermutationDomain PermutationVector;
				PermutationVector.Set<FHardwareRayTraceLightSamplesCS::FHairVoxelTraces>(bHairVoxelTraces);
				PermutationVector.Set<FHardwareRayTraceLightSamplesCS::FDebugMode>(bDebug);
				auto ComputeShader = View.ShaderMap->GetShader<FHardwareRayTraceLightSamplesCS>(PermutationVector);

				FComputeShaderUtils::AddPass(
					GraphBuilder,
					RDG_EVENT_NAME("HardwareRayTraceLightSamples Inline"),
					ERDGPassFlags::Compute,
					ComputeShader,
					PassParameters,
					CompactedTraceParameters.IndirectArgs,
					(int32)StochasticShadows::ECompactedTraceIndirectArgs::NumTracesDiv32);
			}
			else
			{
				FHardwareRayTraceLightSamplesRGS::FParameters* PassParameters = GraphBuilder.AllocParameters<FHardwareRayTraceLightSamplesRGS::FParameters>();
				StochasticShadows::SetHardwareRayTracingPassParameters(
					View,
					GraphBuilder,
					CompactedTraceParameters,
					StochasticShadowsParameters,
					LightSamples,
					LightSampleRayDistance,
					PassParameters);
				PassParameters->HairParameters = HairParameters;

				FHardwareRayTraceLightSamplesRGS::FPermutationDomain PermutationVector;
				PermutationVector.Set<FHardwareRayTraceLightSamplesRGS::FHairVoxelTraces>(bHairVoxelTraces);
				PermutationVector.Set<FHardwareRayTraceLightSamplesRGS::FDebugMode>(bDebug);
				auto RayGenerationShader = View.ShaderMap->GetShader<FHardwareRayTraceLightSamplesRGS>(PermutationVector);

				AddLumenRayTraceDispatchIndirectPass(
					GraphBuilder,
					RDG_EVENT_NAME("HardwareRayTraceLightSamples RayGen"),
					RayGenerationShader,
					PassParameters,
					PassParameters->CompactedTraceParameters.IndirectArgs,
					(int32)StochasticShadows::ECompactedTraceIndirectArgs::NumTraces,
					View,
					/*bUseMinimalPayload*/ true);
			}
			#endif
		}
		else
		{
			ensure(StochasticShadows::IsUsingGlobalSDF());

			FSoftwareRayTraceLightSamplesCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FSoftwareRayTraceLightSamplesCS::FParameters>();
			PassParameters->CompactedTraceParameters = CompactedTraceParameters;
			PassParameters->StochasticShadowsParameters = StochasticShadowsParameters;
			PassParameters->HairParameters = HairParameters;
			PassParameters->RWLightSamples = GraphBuilder.CreateUAV(LightSamples);
			PassParameters->LightSampleRayDistance = LightSampleRayDistance;

			FSoftwareRayTraceLightSamplesCS::FPermutationDomain PermutationVector;
			PermutationVector.Set<FSoftwareRayTraceLightSamplesCS::FHairVoxelTraces>(bHairVoxelTraces);
			PermutationVector.Set<FSoftwareRayTraceLightSamplesCS::FDebugMode>(bDebug);
			auto ComputeShader = View.ShaderMap->GetShader<FSoftwareRayTraceLightSamplesCS>(PermutationVector);

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("SoftwareRayTraceLightSamples"),
				ComputeShader,
				PassParameters,
				CompactedTraceParameters.IndirectArgs,
				0);
		}
	}
}