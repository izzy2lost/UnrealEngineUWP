// Copyright Epic Games, Inc. All Rights Reserved.

#include "StochasticShadows.h"
#include "RendererPrivate.h"
#include "PixelShaderUtils.h"
#include "BlueNoise.h"
#include "BasePassRendering.h"
#include "RayTracing/RayTracingMaterialHitShaders.h"
#include "Lumen/LumenTracingUtils.h"

static TAutoConsoleVariable<int32> CVarStochasticShadows(
	TEXT("r.StochasticShadows"),
	0,
	TEXT("Whether to enable Stochastic Direct Lighting. Experimental feature stochastically sampling analytical light in a single pass by leveraging ray tracing.\n")
	TEXT("1 - all lights using ray tracing shadows will be stochastically sampled\n")
	TEXT("2 - all lights will be stochastically sampled"),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarStochasticShadowsNumSamplesPerPixel(
	TEXT("r.StochasticShadows.NumSamplesPerPixel"),
	4,
	TEXT("Number of samples (shadow rays) per half-res pixel.\n")
	TEXT("1 - 0.25 trace per pixel\n")
	TEXT("2 - 0.5 trace per pixel\n")
	TEXT("4 - 1 trace per pixel"),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> CVarStochasticShadowsMinSampleWeight(
	TEXT("r.StochasticShadows.MinSampleWeight"),
	0.002f,
	TEXT("Determines minimal sample influence on final pixels. Used to skip samples which would have minimal impact to the final image even if light is fully visible."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarStochasticShadowsScreenTraces(
	TEXT("r.StochasticShadows.ScreenTraces"),
	1,
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
	TEXT("Uses hardware inline ray tracing for ray traced lighting, when available.\n"),
	ECVF_RenderThreadSafe | ECVF_Scalability
);

static TAutoConsoleVariable<int32> CVarStochasticShadowsTemporal(
	TEXT("r.StochasticShadows.Temporal"),
	1,
	TEXT("Whether to use temporal accumulation for shadow mask."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarStochasticShadowsGuiding(
	TEXT("r.StochasticShadows.Guiding"),
	1,
	TEXT("Whether to use shadow mask history for sample guiding."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarStochasticShadowsSpatial(
	TEXT("r.StochasticShadows.Spatial"),
	0,
	TEXT("Whether to run spatial shadow mask denoising pass."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarStochasticShadowsWaveOps(
	TEXT("r.StochasticShadows.WaveOps"),
	1,
	TEXT("Whether to use wave ops. Useful for debugging."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarStochasticShadowsDebug(
	TEXT("r.StochasticShadows.Debug"),
	0,
	TEXT("Whether to enabled debug mode, which prints various extra debug information from shaders.")
	TEXT("0 - Disable\n")
	TEXT("1 - Visualize sampling\n")
	TEXT("2 - Visualize tracing\n"),
	ECVF_RenderThreadSafe
);

int32 GStochasticShadowsReset = 0;
FAutoConsoleVariableRef CVarStochasticShadowsReset(
	TEXT("r.StochasticShadows.Reset"),
	GStochasticShadowsReset,
	TEXT("Reset history for debugging."),
	ECVF_RenderThreadSafe
);

int32 GStochasticShadowsResetEveryNthFrame = 0;
	FAutoConsoleVariableRef CVarStochasticShadowsResetEveryNthFrame(
	TEXT("r.StochasticShadows.ResetEveryNthFrame"),
		GStochasticShadowsResetEveryNthFrame,
	TEXT("Reset history every Nth frame for debugging."),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int> CVarStochasticShadowsFixedStateFrameIndex(
	TEXT("r.StochasticShadows.FixedStateFrameIndex"),
	-1,
	TEXT("Whether to override View.StateFrameIndex for debugging."),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int> CVarStochasticShadowsHashTable(
	TEXT("r.StochasticShadows.HashTable"),
	0,
	TEXT("Whether to use bit array limites to 256 lights per frustum or unlimited hash table."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int> CVarStochasticShadowsCandidateLightMask(
	TEXT("r.StochasticShadows.CandidateLightMask"),
	1,
	TEXT("#kris_todo: finish and pick one shader path."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

namespace StochasticShadows
{
	constexpr int32 ShadowMaskTileSize = 8;	// Stored downsampled
	constexpr int32 MaxLightSceneIdXY = 16; // 16 * 16 = 256
	constexpr int32 MaxShadingTilesPerGridCell = 16;
	constexpr int32 ShadowMaskAtlasSizeInTiles = 512;
	constexpr uint32 InvalidShadowMaskTileIndex = 0xFFFFFFFF;
	constexpr uint32 ShadowMaskHashTableFactor = 4; // How much hash table should be larger than max number of elements?

	bool IsEnabled()
	{
		return CVarStochasticShadows.GetValueOnRenderThread() != 0;
	}

	bool IsUsingClosestHZB()
	{
		return IsEnabled() && CVarStochasticShadowsScreenTraces.GetValueOnRenderThread() != 0;
	}

	bool IsLightSupported(uint8 LightType, ECastRayTracedShadow::Type CastRayTracedShadow)
	{
		if (StochasticShadows::IsEnabled() && LightType != LightType_Directional)
		{
			const bool bRayTracedShadows = (CastRayTracedShadow == ECastRayTracedShadow::Enabled || (ShouldRenderRayTracingShadows() && CastRayTracedShadow == ECastRayTracedShadow::UseProjectSetting));
			return CVarStochasticShadows.GetValueOnRenderThread() == 2 || bRayTracedShadows;
		}

		return false;
	}

	bool ShouldCompileShaders(const FGlobalShaderPermutationParameters& Parameters)
	{
		if (IsMobilePlatform(Parameters.Platform))
		{
			return false;
		}

		// SM6 because it uses typed loads to accumulate lights
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM6);
	}

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

	uint32 GetStateFrameIndex(FSceneViewState* ViewState)
	{
		uint32 StateFrameIndex = ViewState ? ViewState->GetFrameIndex() : 0;

		if (CVarStochasticShadowsFixedStateFrameIndex.GetValueOnRenderThread() >= 0)
		{
			StateFrameIndex = CVarStochasticShadowsFixedStateFrameIndex.GetValueOnRenderThread();
		}
		
		return StateFrameIndex;
	}

	uint32 GetNumSamplesPerPixel()
	{
		const uint32 NumSamplesPerPixel = FMath::RoundUpToPowerOfTwo(FMath::Clamp(CVarStochasticShadowsNumSamplesPerPixel.GetValueOnRenderThread(), 1, 4));
		return NumSamplesPerPixel;
	}

	FIntPoint GetNumSamplesPerPixel2d(uint32 NumSamplesPerPixel1d)
	{
		return NumSamplesPerPixel1d == 4 ? FIntPoint(2, 2) : (NumSamplesPerPixel1d == 2 ? FIntPoint(2, 1) : FIntPoint(1, 1));
	}

	enum class ECompactedTraceIndirectArgs
	{
		NumTracesDiv64 = 0 * sizeof(FRHIDispatchIndirectParameters),
		NumTracesDiv32 = 1 * sizeof(FRHIDispatchIndirectParameters),
		NumTraces      = 2 * sizeof(FRHIDispatchIndirectParameters),
		MAX = 3
	};
}

BEGIN_SHADER_PARAMETER_STRUCT(FStochasticShadowsParameters, )
	SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, ViewUniformBuffer)
	SHADER_PARAMETER_STRUCT_INCLUDE(ShaderPrint::FShaderParameters, ShaderPrintUniformBuffer)
	SHADER_PARAMETER_STRUCT_INCLUDE(FSceneTextureParameters, SceneTextures)
	SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSceneUniformParameters, Scene)
	SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSceneTextureUniformParameters, SceneTexturesStruct)
	SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSubstrateGlobalUniformParameters, Substrate)
	SHADER_PARAMETER_STRUCT_REF(FBlueNoise, BlueNoise)
	SHADER_PARAMETER(FIntPoint, SampleViewSize)
	SHADER_PARAMETER(FIntPoint, DownsampledViewSize)
	SHADER_PARAMETER(FIntPoint, ShadowMaskViewSize)
	SHADER_PARAMETER(uint32, StochasticShadowsStateFrameIndex)
	SHADER_PARAMETER(uint32, MaxShadowMaskTiles)
	SHADER_PARAMETER(uint32, MaxShadingTiles)
	SHADER_PARAMETER(uint32, MaxShadingTilesPerGridCell)
	SHADER_PARAMETER(uint32, ShadowMaskHashTableIndexWrapMask)
	SHADER_PARAMETER(FIntPoint, ShadowMaskPageTablePerLightSize)
	SHADER_PARAMETER(FIntPoint, ShadingTileGridSize)
	SHADER_PARAMETER(FVector2f, DownsampledBufferInvSize)
	SHADER_PARAMETER(float, MinLightSampleWeight)
	SHADER_PARAMETER(int32, DebugMode)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float>, DownsampledSceneDepth)
END_SHADER_PARAMETER_STRUCT()

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

class FInitShadowMaskUpdateIndirectArgsCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FInitShadowMaskUpdateIndirectArgsCS)
	SHADER_USE_PARAMETER_STRUCT(FInitShadowMaskUpdateIndirectArgsCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FStochasticShadowsParameters, StochasticShadowsParameters)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWIndirectArgs)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, ShadowMaskTileAllocator)
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

IMPLEMENT_GLOBAL_SHADER(FInitShadowMaskUpdateIndirectArgsCS, "/Engine/Private/StochasticShadows/StochasticShadows.usf", "InitShadowMaskUpdateIndirectArgsCS", SF_Compute);

class FGenerateSamplesCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FGenerateSamplesCS)
	SHADER_USE_PARAMETER_STRUCT(FGenerateSamplesCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FStochasticShadowsParameters, StochasticShadowsParameters)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float>, RWDownsampledSceneDepth)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<uint32>, RWShadowMaskHistoryScreenCoord00)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, RWShadowMaskHistoryWeights)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, RWShadowMaskTileAllocator)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint2>, RWShadowMaskTileHeader)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<uint>, RWShadowMaskPageTable)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, RWShadowMaskHashTable)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<uint>, RWLightSamples)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FForwardLightData, ForwardLightData)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<uint>, ShadowMaskPageTableHistory)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, ShadowMaskHashTableHistory)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<uint>, ShadowMaskAtlasHistory)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<uint>, ShadowMaskTileAtlas)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float>, ShadowMaskSceneDepthHistory)
		SHADER_PARAMETER(FVector4f, HistoryUVMinMax)
		SHADER_PARAMETER(FVector4f, HistoryScreenPositionScaleBias)
	END_SHADER_PARAMETER_STRUCT()

	class FNumSamplesPerPixel : SHADER_PERMUTATION_SPARSE_INT("NUM_SAMPLES_PER_PIXEL", 1, 2, 4);
	class FShadowMaskReprojectionWeights : SHADER_PERMUTATION_BOOL("SHADOW_MASK_REPROJECTION_WEIGHTS");
	class FShadowFactorEstimate : SHADER_PERMUTATION_BOOL("SHADOW_FACTOR_ESTIMATE");
	class FHashTable : SHADER_PERMUTATION_BOOL("HASH_TABLE");
	class FCandidateLightMask : SHADER_PERMUTATION_BOOL("CANDIDATE_LIGHT_MASK");
	class FDebugMode : SHADER_PERMUTATION_BOOL("DEBUG_MODE");
	using FPermutationDomain = TShaderPermutationDomain<FNumSamplesPerPixel, FShadowMaskReprojectionWeights, FShadowFactorEstimate, FHashTable, FCandidateLightMask, FDebugMode>;

	static int32 GetGroupSize()
	{	
		return 8;
	}

	static FPermutationDomain RemapPermutation(FPermutationDomain PermutationVector)
	{
		if (PermutationVector.Get<FShadowFactorEstimate>())
		{
			PermutationVector.Set<FShadowMaskReprojectionWeights>(true);
		}

		return PermutationVector;
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		FPermutationDomain PermutationVector(Parameters.PermutationId);

		if (RemapPermutation(PermutationVector) != PermutationVector)
		{
			return false;
		}

		return StochasticShadows::ShouldCompileShaders(Parameters);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		FForwardLightingParameters::ModifyCompilationEnvironment(Parameters.Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());

		FPermutationDomain PermutationVector(Parameters.PermutationId);
		const uint32 NumSamplesPerPixel = PermutationVector.Get<FNumSamplesPerPixel>();
		OutEnvironment.SetDefine(TEXT("NUM_SAMPLES_PER_PIXEL_X"), StochasticShadows::GetNumSamplesPerPixel2d(NumSamplesPerPixel).X);
		OutEnvironment.SetDefine(TEXT("NUM_SAMPLES_PER_PIXEL_Y"), StochasticShadows::GetNumSamplesPerPixel2d(NumSamplesPerPixel).Y);
	}
};

IMPLEMENT_GLOBAL_SHADER(FGenerateSamplesCS, "/Engine/Private/StochasticShadows/StochasticShadowsSampling.usf", "GenerateSamplesCS", SF_Compute);

BEGIN_SHADER_PARAMETER_STRUCT(FCompactedStochasticShadowsTraceParameters, )
	RDG_BUFFER_ACCESS(IndirectArgs, ERHIAccess::IndirectArgs)
	SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, CompactedTraceTexelData)
	SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, CompactedTraceTexelAllocator)
END_SHADER_PARAMETER_STRUCT()

#if RHI_RAYTRACING

class FHardwareRayTraceLightSamplesCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FHardwareRayTraceLightSamplesCS)
	SHADER_USE_PARAMETER_STRUCT(FHardwareRayTraceLightSamplesCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FCompactedStochasticShadowsTraceParameters, CompactedTraceParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(FStochasticShadowsParameters, StochasticShadowsParameters)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<uint>, RWLightSamples)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float>, LightSampleRayDistance)
		SHADER_PARAMETER_RDG_BUFFER_SRV(RaytracingAccelerationStructure, TLAS)
	END_SHADER_PARAMETER_STRUCT()

	class FNumSamplesPerPixel : SHADER_PERMUTATION_SPARSE_INT("NUM_SAMPLES_PER_PIXEL", 1, 2, 4);
	class FDebugMode : SHADER_PERMUTATION_BOOL("DEBUG_MODE");
	using FPermutationDomain = TShaderPermutationDomain<FNumSamplesPerPixel, FDebugMode>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return StochasticShadows::ShouldCompileShaders(Parameters)
			&& ShouldCompileRayTracingShadersForProject(Parameters.Platform)
			&& FDataDrivenShaderPlatformInfo::GetSupportsInlineRayTracing(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

		OutEnvironment.CompilerFlags.Add(CFLAG_Wave32);	
		OutEnvironment.CompilerFlags.Add(CFLAG_InlineRayTracing);

		OutEnvironment.SetDefine(TEXT("INLINE_RAY_TRACING_THREAD_GROUP_SIZE_X"), GetGroupSize().X);
		OutEnvironment.SetDefine(TEXT("INLINE_RAY_TRACING_THREAD_GROUP_SIZE_Y"), GetGroupSize().Y);

		FPermutationDomain PermutationVector(Parameters.PermutationId);
		const uint32 NumSamplesPerPixel = PermutationVector.Get<FNumSamplesPerPixel>();
		OutEnvironment.SetDefine(TEXT("NUM_SAMPLES_PER_PIXEL_X"), StochasticShadows::GetNumSamplesPerPixel2d(NumSamplesPerPixel).X);
		OutEnvironment.SetDefine(TEXT("NUM_SAMPLES_PER_PIXEL_Y"), StochasticShadows::GetNumSamplesPerPixel2d(NumSamplesPerPixel).Y);
	}

	static FIntPoint GetGroupSize()
	{
		// Current inline ray tracing implementation requires 1:1 mapping between thread groups and waves and only supports wave32 mode.
		return FIntPoint(32, 1);
	}
};

IMPLEMENT_GLOBAL_SHADER(FHardwareRayTraceLightSamplesCS, "/Engine/Private/StochasticShadows/StochasticShadowsHardwareRayTracing.usf", "HardwareRayTraceLightSamplesCS", SF_Compute);

class FHardwareRayTraceLightSamplesRGS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FHardwareRayTraceLightSamplesRGS)
	SHADER_USE_ROOT_PARAMETER_STRUCT(FHardwareRayTraceLightSamplesRGS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FCompactedStochasticShadowsTraceParameters, CompactedTraceParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(FStochasticShadowsParameters, StochasticShadowsParameters)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<uint>, RWLightSamples)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float>, LightSampleRayDistance)
		SHADER_PARAMETER_RDG_BUFFER_SRV(RaytracingAccelerationStructure, TLAS)
	END_SHADER_PARAMETER_STRUCT()

	static ERayTracingPayloadType GetRayTracingPayloadType(const int32 PermutationId)
	{
		return ERayTracingPayloadType::RayTracingMaterial;
	}

	class FNumSamplesPerPixel : SHADER_PERMUTATION_SPARSE_INT("NUM_SAMPLES_PER_PIXEL", 1, 2, 4);
	class FDebugMode : SHADER_PERMUTATION_BOOL("DEBUG_MODE");
	using FPermutationDomain = TShaderPermutationDomain<FNumSamplesPerPixel, FDebugMode>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return StochasticShadows::ShouldCompileShaders(Parameters)
			&& ShouldCompileRayTracingShadersForProject(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("UE_RAY_TRACING_DYNAMIC_CLOSEST_HIT_SHADER"), 0);
		OutEnvironment.SetDefine(TEXT("UE_RAY_TRACING_DYNAMIC_ANY_HIT_SHADER"), 1);
		OutEnvironment.SetDefine(TEXT("UE_RAY_TRACING_DYNAMIC_MISS_SHADER"), 0);
		OutEnvironment.SetDefine(TEXT("UE_RAY_TRACING_DISPATCH_1D"), 1);

		FPermutationDomain PermutationVector(Parameters.PermutationId);
		const uint32 NumSamplesPerPixel = PermutationVector.Get<FNumSamplesPerPixel>();
		OutEnvironment.SetDefine(TEXT("NUM_SAMPLES_PER_PIXEL_X"), StochasticShadows::GetNumSamplesPerPixel2d(NumSamplesPerPixel).X);
		OutEnvironment.SetDefine(TEXT("NUM_SAMPLES_PER_PIXEL_Y"), StochasticShadows::GetNumSamplesPerPixel2d(NumSamplesPerPixel).Y);
	}
};

IMPLEMENT_GLOBAL_SHADER(FHardwareRayTraceLightSamplesRGS, "/Engine/Private/StochasticShadows/StochasticShadowsHardwareRayTracing.usf", "HardwareRayTraceLightSamplesRGS", SF_RayGen);

#endif // RHI_RAYTRACING

class FSoftwareRayTraceLightSamplesCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FSoftwareRayTraceLightSamplesCS)
	SHADER_USE_PARAMETER_STRUCT(FSoftwareRayTraceLightSamplesCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FCompactedStochasticShadowsTraceParameters, CompactedTraceParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(FStochasticShadowsParameters, StochasticShadowsParameters)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<uint>, RWLightSamples)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float>, LightSampleRayDistance)
	END_SHADER_PARAMETER_STRUCT()

	static int32 GetGroupSize()
	{
		return 64;
	}

	class FNumSamplesPerPixel : SHADER_PERMUTATION_SPARSE_INT("NUM_SAMPLES_PER_PIXEL", 1, 2, 4);
	class FDebugMode : SHADER_PERMUTATION_BOOL("DEBUG_MODE");
	using FPermutationDomain = TShaderPermutationDomain<FNumSamplesPerPixel, FDebugMode>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return StochasticShadows::ShouldCompileShaders(Parameters);
	}

	FORCENOINLINE static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.CompilerFlags.Add(CFLAG_Wave32);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());

		FPermutationDomain PermutationVector(Parameters.PermutationId);
		const uint32 NumSamplesPerPixel = PermutationVector.Get<FNumSamplesPerPixel>();
		OutEnvironment.SetDefine(TEXT("NUM_SAMPLES_PER_PIXEL_X"), StochasticShadows::GetNumSamplesPerPixel2d(NumSamplesPerPixel).X);
		OutEnvironment.SetDefine(TEXT("NUM_SAMPLES_PER_PIXEL_Y"), StochasticShadows::GetNumSamplesPerPixel2d(NumSamplesPerPixel).Y);
	}
};

IMPLEMENT_GLOBAL_SHADER(FSoftwareRayTraceLightSamplesCS, "/Engine/Private/StochasticShadows/StochasticShadowsTracing.usf", "SoftwareRayTraceLightSamplesCS", SF_Compute);

class FScreenSpaceRayTraceLightSamplesCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FScreenSpaceRayTraceLightSamplesCS)
	SHADER_USE_PARAMETER_STRUCT(FScreenSpaceRayTraceLightSamplesCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FCompactedStochasticShadowsTraceParameters, CompactedTraceParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(FStochasticShadowsParameters, StochasticShadowsParameters)
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

	class FNumSamplesPerPixel : SHADER_PERMUTATION_SPARSE_INT("NUM_SAMPLES_PER_PIXEL", 1, 2, 4);
	class FDebugMode : SHADER_PERMUTATION_BOOL("DEBUG_MODE");
	using FPermutationDomain = TShaderPermutationDomain<FNumSamplesPerPixel, FDebugMode>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return StochasticShadows::ShouldCompileShaders(Parameters);
	}

	FORCENOINLINE static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.CompilerFlags.Add(CFLAG_Wave32);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());

		FPermutationDomain PermutationVector(Parameters.PermutationId);
		const uint32 NumSamplesPerPixel = PermutationVector.Get<FNumSamplesPerPixel>();
		OutEnvironment.SetDefine(TEXT("NUM_SAMPLES_PER_PIXEL_X"), StochasticShadows::GetNumSamplesPerPixel2d(NumSamplesPerPixel).X);
		OutEnvironment.SetDefine(TEXT("NUM_SAMPLES_PER_PIXEL_Y"), StochasticShadows::GetNumSamplesPerPixel2d(NumSamplesPerPixel).Y);
	}
};

IMPLEMENT_GLOBAL_SHADER(FScreenSpaceRayTraceLightSamplesCS, "/Engine/Private/StochasticShadows/StochasticShadowsTracing.usf", "ScreenSpaceRayTraceLightSamplesCS", SF_Compute);

class FInitShadowMaskUpsampleWeightsCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FInitShadowMaskUpsampleWeightsCS)
	SHADER_USE_PARAMETER_STRUCT(FInitShadowMaskUpsampleWeightsCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FStochasticShadowsParameters, StochasticShadowsParameters)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, RWShadowMaskUpsampleWeights)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float>, ShadowMaskSceneDepthHistory)
		SHADER_PARAMETER(FVector4f, HistoryUVMinMax)
		SHADER_PARAMETER(FVector4f, HistoryScreenPositionScaleBias)
	END_SHADER_PARAMETER_STRUCT()

	static int32 GetGroupSize()
	{
		return 8;
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return StochasticShadows::ShouldCompileShaders(Parameters);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());
	}
};

IMPLEMENT_GLOBAL_SHADER(FInitShadowMaskUpsampleWeightsCS, "/Engine/Private/StochasticShadows/StochasticShadows.usf", "InitShadowMaskUpsampleWeightsCS", SF_Compute);

class FCompositeShadowMaskTracesCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FCompositeShadowMaskTracesCS)
	SHADER_USE_PARAMETER_STRUCT(FCompositeShadowMaskTracesCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FStochasticShadowsParameters, StochasticShadowsParameters)
		RDG_BUFFER_ACCESS(IndirectArgs, ERHIAccess::IndirectArgs)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<uint>, RWShadowMaskTileAtlas)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<uint>, ShadowMaskPageTable)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint2>, ShadowMaskTileHeader)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<uint32>, ShadowMaskHistoryScreenCoord00)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float4>, ShadowMaskHistoryWeights)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<uint>, ShadowMaskPageTableHistory)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, ShadowMaskHashTableHistory)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<uint>, ShadowMaskAtlasHistory)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<uint>, LightSamples)
	END_SHADER_PARAMETER_STRUCT()

	static int32 GetGroupSize()
	{
		return 8;
	}

	class FNumSamplesPerPixel : SHADER_PERMUTATION_SPARSE_INT("NUM_SAMPLES_PER_PIXEL", 1, 2, 4);
	class FTemporalAccumulation : SHADER_PERMUTATION_BOOL("TEMPORAL_ACCUMULATION");
	class FHashTable : SHADER_PERMUTATION_BOOL("HASH_TABLE");
	using FPermutationDomain = TShaderPermutationDomain<FNumSamplesPerPixel, FTemporalAccumulation, FHashTable>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return StochasticShadows::ShouldCompileShaders(Parameters);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());

		FPermutationDomain PermutationVector(Parameters.PermutationId);
		const uint32 NumSamplesPerPixel = PermutationVector.Get<FNumSamplesPerPixel>();
		OutEnvironment.SetDefine(TEXT("NUM_SAMPLES_PER_PIXEL_X"), StochasticShadows::GetNumSamplesPerPixel2d(NumSamplesPerPixel).X);
		OutEnvironment.SetDefine(TEXT("NUM_SAMPLES_PER_PIXEL_Y"), StochasticShadows::GetNumSamplesPerPixel2d(NumSamplesPerPixel).Y);
	}
};

IMPLEMENT_GLOBAL_SHADER(FCompositeShadowMaskTracesCS, "/Engine/Private/StochasticShadows/StochasticShadows.usf", "CompositeShadowMaskTracesCS", SF_Compute);

class FShadowMaskSpatialPassCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FShadowMaskSpatialPassCS)
	SHADER_USE_PARAMETER_STRUCT(FShadowMaskSpatialPassCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FStochasticShadowsParameters, StochasticShadowsParameters)
		RDG_BUFFER_ACCESS(IndirectArgs, ERHIAccess::IndirectArgs)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, RWShadingTileAllocator)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<uint>, RWShadingTileGridAllocator)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, RWShadingTileGrid)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<UNORM float>, RWShadingTileAtlas)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float4>, ShadowMaskUpsampleWeights)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint2>, ShadowMaskTileHeader)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<uint>, ShadowMaskPageTable)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<uint>, ShadowMaskTileAtlas)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, ShadowMaskHashTable)
	END_SHADER_PARAMETER_STRUCT()

	static int32 GetGroupSize()
	{
		return 8;
	}

	class FHashTable : SHADER_PERMUTATION_BOOL("HASH_TABLE");
	class FSpatialPass : SHADER_PERMUTATION_BOOL("SPATIAL_PASS");
	using FPermutationDomain = TShaderPermutationDomain<FHashTable, FSpatialPass>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return StochasticShadows::ShouldCompileShaders(Parameters);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());
	}
};

IMPLEMENT_GLOBAL_SHADER(FShadowMaskSpatialPassCS, "/Engine/Private/StochasticShadows/StochasticShadowsSpatial.usf", "ShadowMaskSpatialPassCS", SF_Compute);

class FShadeLightSamplesWithShadowMaskCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FShadeLightSamplesWithShadowMaskCS)
	SHADER_USE_PARAMETER_STRUCT(FShadeLightSamplesWithShadowMaskCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FStochasticShadowsParameters, StochasticShadowsParameters)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, RWSceneColor)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<uint>, ShadowMaskPageTable)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<uint>, ShadowMaskTileAtlas)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, ShadowMaskTileAllocator)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, ShadingTileAllocator)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<uint>, ShadingTileGridAllocator)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, ShadingTileGrid)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, ShadingTiles)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<UNORM float>, ShadingTileAtlas)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, ShadowMaskHashTable)
	END_SHADER_PARAMETER_STRUCT()

	static int32 GetGroupSize()
	{
		return 8;
	}

	class FDebugMode : SHADER_PERMUTATION_BOOL("DEBUG_MODE");
	using FPermutationDomain = TShaderPermutationDomain<FDebugMode>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return StochasticShadows::ShouldCompileShaders(Parameters);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());
	}
};

IMPLEMENT_GLOBAL_SHADER(FShadeLightSamplesWithShadowMaskCS, "/Engine/Private/StochasticShadows/StochasticShadowsShading.usf", "ShadeLightSamplesWithShadowMaskCS", SF_Compute);

#if RHI_RAYTRACING
void FDeferredShadingSceneRenderer::PrepareStochasticShadows(const FViewInfo& View, const FScene& Scene, TArray<FRHIRayTracingShader*>& OutRayGenShaders)
{

	if (StochasticShadows::IsEnabled())
	{
		FHardwareRayTraceLightSamplesRGS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FHardwareRayTraceLightSamplesRGS::FNumSamplesPerPixel>(StochasticShadows::GetNumSamplesPerPixel());
		PermutationVector.Set<FHardwareRayTraceLightSamplesRGS::FDebugMode>(CVarStochasticShadowsDebug.GetValueOnRenderThread() != 0);
		TShaderRef<FHardwareRayTraceLightSamplesRGS> RayGenerationShader = View.ShaderMap->GetShader<FHardwareRayTraceLightSamplesRGS>(PermutationVector);
		OutRayGenShaders.Add(RayGenerationShader.GetRayTracingShader());
	}
}
#endif

FCompactedStochasticShadowsTraceParameters CompactStochasticShadowsTraces(
	const FViewInfo& View,
	FRDGBuilder& GraphBuilder,
	const FIntPoint SampleBufferSize,
	FRDGTextureRef LightSamples,
	FStochasticShadowsParameters StochasticShadowsParameters)
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

		const bool bWaveOps = CVarStochasticShadowsWaveOps.GetValueOnRenderThread() != 0
			&& GRHISupportsWaveOperations
			&& GRHIMinimumWaveSize <= 32
			&& GRHIMaximumWaveSize >= 32
			&& RHISupportsWaveOperations(View.GetShaderPlatform());

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

	FCompactedStochasticShadowsTraceParameters Parameters;
	Parameters.CompactedTraceTexelAllocator = GraphBuilder.CreateSRV(CompactedTraceTexelAllocator);
	Parameters.CompactedTraceTexelData = GraphBuilder.CreateSRV(CompactedTraceTexelData);
	Parameters.IndirectArgs = CompactedTraceTexelIndirectArgs;
	return Parameters;
}

/**
 * Single pass batched light rendering using ray tracing (distance field or triangle) for shadowing.
 */
void FDeferredShadingSceneRenderer::RenderStochasticShadows(FRDGBuilder& GraphBuilder, const FSceneTextures& SceneTextures)
{
	if (!StochasticShadows::IsEnabled())
	{
		return;
	}

	RDG_EVENT_SCOPE(GraphBuilder, "StochasticShadows");

	const FViewInfo& View = Views[0];
	FBlueNoise BlueNoise = GetBlueNoiseGlobalParameters();
	TUniformBufferRef<FBlueNoise> BlueNoiseUniformBuffer = CreateUniformBufferImmediate(BlueNoise, EUniformBufferUsage::UniformBuffer_SingleDraw);

	const bool bDebug = CVarStochasticShadowsDebug.GetValueOnRenderThread() != 0;
	const bool bWaveOps = CVarStochasticShadowsWaveOps.GetValueOnRenderThread() != 0
		&& GRHISupportsWaveOperations
		&& GRHIMinimumWaveSize <= 32
		&& GRHIMaximumWaveSize >= 32
		&& RHISupportsWaveOperations(View.GetShaderPlatform());

	// History reset for debugging purposes
	bool bResetHistory = false;

	if (GStochasticShadowsResetEveryNthFrame > 0 && (ViewFamily.FrameNumber % (uint32)GStochasticShadowsResetEveryNthFrame) == 0)
	{
		bResetHistory = true;
	}

	if (GStochasticShadowsReset != 0)
	{
		GStochasticShadowsReset = 0;
		bResetHistory = true;
	}

	const int32 NumSamplesPerPixel1d = StochasticShadows::GetNumSamplesPerPixel();
	const FIntPoint NumSamplesPerPixel2d = StochasticShadows::GetNumSamplesPerPixel2d(NumSamplesPerPixel1d);

	const uint32 DownsampleFactor = 2;
	const uint32 ShadowMaskDownsampleFactor = 2;
	const FIntPoint DownsampledViewSize = FIntPoint::DivideAndRoundUp(View.ViewRect.Size(), DownsampleFactor);
	const FIntPoint SampleViewSize = DownsampledViewSize * NumSamplesPerPixel2d;
	const FIntPoint DownsampledBufferSize = FIntPoint::DivideAndRoundUp(SceneTextures.Config.Extent, DownsampleFactor);
	const FIntPoint SampleBufferSize = DownsampledBufferSize * NumSamplesPerPixel2d;
	const FIntPoint DonwnsampledSampleBufferSize = DownsampledBufferSize * NumSamplesPerPixel2d;

	const FIntPoint ShadowMaskBufferSize = FIntPoint::DivideAndRoundUp(SceneTextures.Config.Extent, ShadowMaskDownsampleFactor);
	const FIntPoint ShadowMaskViewSize = FIntPoint::DivideAndRoundUp(View.ViewRect.Size(), ShadowMaskDownsampleFactor);
	const FIntPoint ShadowMaskPageTablePerLightSize = FIntPoint::DivideAndRoundUp(ShadowMaskBufferSize, StochasticShadows::ShadowMaskTileSize);
	const FIntPoint ShadowMaskPageTableSize = ShadowMaskPageTablePerLightSize * StochasticShadows::MaxLightSceneIdXY;
	const FIntPoint ShadowMaskTileAtlasSize = StochasticShadows::ShadowMaskTileSize * StochasticShadows::ShadowMaskAtlasSizeInTiles;
	const int32 MaxShadowMaskTiles = StochasticShadows::ShadowMaskAtlasSizeInTiles * StochasticShadows::ShadowMaskAtlasSizeInTiles;
	const int32 ShadowMaskHashTableSize = FMath::RoundUpToPowerOfTwo(MaxShadowMaskTiles * StochasticShadows::ShadowMaskHashTableFactor);

	const FIntPoint ShadingTileGridSize = FIntPoint::DivideAndRoundUp(SceneTextures.Config.Extent, StochasticShadows::ShadowMaskTileSize);
	const FIntPoint ShadingTileAtlasSize = ShadowMaskTileAtlasSize;

	FRDGTextureRef DownsampledSceneDepth = GraphBuilder.CreateTexture(
		FRDGTextureDesc::Create2D(DownsampledBufferSize, PF_R32_FLOAT, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV),
		TEXT("StochasticShadows.DownsampledSceneDepth"));

	FRDGTextureRef LightSamples = GraphBuilder.CreateTexture(
		FRDGTextureDesc::Create2D(DonwnsampledSampleBufferSize, PF_R16_UINT, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV),
		TEXT("StochasticShadows.LightSamples"));

	FRDGTextureRef LightSampleRayDistance = GraphBuilder.CreateTexture(
		FRDGTextureDesc::Create2D(DonwnsampledSampleBufferSize, PF_R16F, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV),
		TEXT("StochasticShadows.LightSampleRayDistance"));

	const bool bUseHashTable = CVarStochasticShadowsHashTable.GetValueOnRenderThread() != 0;
	
	FRDGBufferRef ShadowMaskHashTable = nullptr;
	FRDGTextureRef ShadowMaskPageTable = nullptr;

	if (bUseHashTable)
	{
		ShadowMaskHashTable = GraphBuilder.CreateBuffer(
			FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), ShadowMaskHashTableSize),
			TEXT("StochasticShadows.ShadowMaskTableMap"));

		AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(ShadowMaskHashTable), StochasticShadows::InvalidShadowMaskTileIndex);
	}
	else
	{
		ShadowMaskPageTable = GraphBuilder.CreateTexture(
			FRDGTextureDesc::Create2D(ShadowMaskPageTableSize, PF_R32_UINT, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV),
			TEXT("StochasticShadows.ShadowMaskPageTable"));

		AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(ShadowMaskPageTable), StochasticShadows::InvalidShadowMaskTileIndex);
	}

	const EPixelFormat ShadowMaskTileAtlasFormat = PF_R32_UINT;
	FRDGTextureRef ShadowMaskTileAtlas = GraphBuilder.CreateTexture(
		FRDGTextureDesc::Create2D(ShadowMaskTileAtlasSize, ShadowMaskTileAtlasFormat, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV),
		TEXT("StochasticShadows.ShadowMaskTileAtlas"));

	FStochasticShadowsParameters StochasticShadowsParameters;
	{
		StochasticShadowsParameters.ViewUniformBuffer = View.ViewUniformBuffer;
		StochasticShadowsParameters.Scene = View.GetSceneUniforms().GetBuffer(GraphBuilder);
		StochasticShadowsParameters.SceneTextures = GetSceneTextureParameters(GraphBuilder, SceneTextures.UniformBuffer);
		StochasticShadowsParameters.SceneTexturesStruct = SceneTextures.UniformBuffer;
		StochasticShadowsParameters.Substrate = Substrate::BindSubstrateGlobalUniformParameters(View);
		StochasticShadowsParameters.BlueNoise = BlueNoiseUniformBuffer;
		StochasticShadowsParameters.DownsampledViewSize = DownsampledViewSize;
		StochasticShadowsParameters.SampleViewSize = SampleViewSize;
		StochasticShadowsParameters.ShadowMaskViewSize = ShadowMaskViewSize;
		StochasticShadowsParameters.StochasticShadowsStateFrameIndex = StochasticShadows::GetStateFrameIndex(View.ViewState);
		StochasticShadowsParameters.DownsampledSceneDepth = DownsampledSceneDepth;
		StochasticShadowsParameters.MaxShadowMaskTiles = MaxShadowMaskTiles;
		StochasticShadowsParameters.MaxShadingTiles = (ShadingTileAtlasSize.X * ShadingTileAtlasSize.Y) / (StochasticShadows::ShadowMaskTileSize * StochasticShadows::ShadowMaskTileSize);
		StochasticShadowsParameters.MaxShadingTilesPerGridCell = StochasticShadows::MaxShadingTilesPerGridCell;
		StochasticShadowsParameters.ShadingTileGridSize = ShadingTileGridSize;
		StochasticShadowsParameters.ShadowMaskHashTableIndexWrapMask = ShadowMaskHashTableSize - 1;
		StochasticShadowsParameters.ShadowMaskPageTablePerLightSize = ShadowMaskPageTablePerLightSize;
		StochasticShadowsParameters.DownsampledBufferInvSize = FVector2f(1.0f) / DownsampledBufferSize;
		StochasticShadowsParameters.MinLightSampleWeight = CVarStochasticShadowsMinSampleWeight.GetValueOnRenderThread();
		StochasticShadowsParameters.DebugMode = CVarStochasticShadowsDebug.GetValueOnRenderThread();

		if (bDebug)
		{
			ShaderPrint::SetEnabled(true);
			ShaderPrint::RequestSpaceForLines(1024);
			ShaderPrint::SetParameters(GraphBuilder, View.ShaderPrintData, StochasticShadowsParameters.ShaderPrintUniformBuffer);
		}
	}
	
	bool bTemporal = CVarStochasticShadowsTemporal.GetValueOnRenderThread() != 0;
	FRDGTextureRef ShadowMaskSceneDepthHistory = nullptr;
	FRDGTextureRef ShadowMaskPageTableHistory = nullptr;
	FRDGBufferRef ShadowMaskHashTableHistory = nullptr;
	FRDGTextureRef ShadowMaskAtlasHistory = nullptr;
	FVector4f HistoryScreenPositionScaleBias = FVector4f(0.0f, 0.0f, 0.0f, 0.0f);
	FVector4f HistoryUVMinMax = FVector4f(0.0f, 0.0f, 0.0f, 0.0f);

	if (View.ViewState)
	{
		const FStochasticShadowsViewState& LightingViewState = View.ViewState->StochasticShadows;

		if (!View.bCameraCut && !bResetHistory && bTemporal)
		{
			HistoryScreenPositionScaleBias = LightingViewState.HistoryScreenPositionScaleBias;
			HistoryUVMinMax = LightingViewState.HistoryUVMinMax;

			if (LightingViewState.ShadowMaskPageTableHistory 
				&& LightingViewState.ShadowMaskPageTableHistory->GetDesc().Extent == ShadowMaskPageTableSize)
			{
				ShadowMaskPageTableHistory = GraphBuilder.RegisterExternalTexture(LightingViewState.ShadowMaskPageTableHistory);
			}

			// #kris_todo: fix it
			if (LightingViewState.ShadowMaskHashTableHistory 
				/* && LightingViewState.ShadowMaskHashTableHistory->GetAlignedDesc().NumElements == ShadowMaskHashTableSize*/)
			{
				ShadowMaskHashTableHistory = GraphBuilder.RegisterExternalBuffer(LightingViewState.ShadowMaskHashTableHistory);
			}

			if (LightingViewState.ShadowMaskAtlasHistory 
				&& LightingViewState.ShadowMaskAtlasHistory->GetDesc().Extent == ShadowMaskTileAtlasSize)
			{
				ShadowMaskAtlasHistory = GraphBuilder.RegisterExternalTexture(LightingViewState.ShadowMaskAtlasHistory);
				ShadowMaskSceneDepthHistory = LightingViewState.ShadowMaskSceneDepthHistory ? GraphBuilder.RegisterExternalTexture(LightingViewState.ShadowMaskSceneDepthHistory) : nullptr;
			}
		}
	}

	if (bUseHashTable)
	{
		if (ShadowMaskAtlasHistory == nullptr || ShadowMaskHashTableHistory == nullptr)
		{
			bTemporal = false;
		}
	}
	else
	{
		if (ShadowMaskAtlasHistory == nullptr || ShadowMaskPageTableHistory == nullptr)
		{
			bTemporal = false;
		}
	}

	FRDGTextureRef ShadowMaskHistoryWeights = GraphBuilder.CreateTexture(
		FRDGTextureDesc::Create2D(ShadowMaskBufferSize, PF_FloatRGBA, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV),
		TEXT("StochasticShadows.ShadowMaskHistoryWeights"));

	FRDGTextureRef ShadowMaskHistoryScreenCoord00 = GraphBuilder.CreateTexture(
		FRDGTextureDesc::Create2D(ShadowMaskBufferSize, PF_R16G16_UINT, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV),
		TEXT("StochasticShadows.ShadowMaskHistoryScreenCoord00"));

	FRDGBufferRef ShadowMaskTileAllocator = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), 1), TEXT("StochasticShadowsParameters.ShadowMaskTileAllocator"));
	FRDGBufferRef ShadowMaskTileHeader = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(2 * sizeof(uint32), MaxShadowMaskTiles), TEXT("StochasticShadowsParameters.ShadowMaskTileHeader"));
	AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(ShadowMaskTileAllocator), 0);
	
	// Generate new candidate light samples
	{
		FGenerateSamplesCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FGenerateSamplesCS::FParameters>();
		PassParameters->StochasticShadowsParameters = StochasticShadowsParameters;
		PassParameters->RWDownsampledSceneDepth = GraphBuilder.CreateUAV(DownsampledSceneDepth);
		PassParameters->RWShadowMaskHistoryScreenCoord00 = GraphBuilder.CreateUAV(ShadowMaskHistoryScreenCoord00);
		PassParameters->RWShadowMaskHistoryWeights = GraphBuilder.CreateUAV(ShadowMaskHistoryWeights);
		PassParameters->RWShadowMaskTileAllocator = GraphBuilder.CreateUAV(ShadowMaskTileAllocator);
		PassParameters->RWShadowMaskTileHeader = GraphBuilder.CreateUAV(ShadowMaskTileHeader);
		PassParameters->RWShadowMaskPageTable = ShadowMaskPageTable ? GraphBuilder.CreateUAV(ShadowMaskPageTable) : nullptr;
		PassParameters->RWShadowMaskHashTable = ShadowMaskHashTable ? GraphBuilder.CreateUAV(ShadowMaskHashTable) : nullptr;
		PassParameters->RWLightSamples = GraphBuilder.CreateUAV(LightSamples);
		PassParameters->ForwardLightData = View.ForwardLightingResources.ForwardLightUniformBuffer;
		PassParameters->ShadowMaskPageTableHistory = ShadowMaskPageTableHistory;
		PassParameters->ShadowMaskHashTableHistory = ShadowMaskHashTableHistory ? GraphBuilder.CreateSRV(ShadowMaskHashTableHistory) : nullptr;
		PassParameters->ShadowMaskAtlasHistory = ShadowMaskAtlasHistory;
		PassParameters->ShadowMaskTileAtlas = ShadowMaskTileAtlas;
		PassParameters->ShadowMaskSceneDepthHistory = ShadowMaskSceneDepthHistory;
		PassParameters->HistoryScreenPositionScaleBias = HistoryScreenPositionScaleBias;
		PassParameters->HistoryUVMinMax = HistoryUVMinMax;

		FGenerateSamplesCS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FGenerateSamplesCS::FNumSamplesPerPixel>(NumSamplesPerPixel1d);
		PermutationVector.Set<FGenerateSamplesCS::FShadowMaskReprojectionWeights>(bTemporal);
		PermutationVector.Set<FGenerateSamplesCS::FShadowFactorEstimate>(bTemporal && CVarStochasticShadowsGuiding.GetValueOnRenderThread() != 0);
		PermutationVector.Set<FGenerateSamplesCS::FDebugMode>(bDebug);
		PermutationVector.Set<FGenerateSamplesCS::FHashTable>(bUseHashTable);
		PermutationVector.Set<FGenerateSamplesCS::FCandidateLightMask>(ShadowMaskAtlasHistory && CVarStochasticShadowsCandidateLightMask.GetValueOnRenderThread() != 0);
		PermutationVector = FGenerateSamplesCS::RemapPermutation(PermutationVector);
		auto ComputeShader = View.ShaderMap->GetShader<FGenerateSamplesCS>(PermutationVector);

		const FIntVector GroupCount = FComputeShaderUtils::GetGroupCount(DownsampledViewSize, FGenerateSamplesCS::GetGroupSize());

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("GenerateSamples SamplesPerPixel:%dx%d", NumSamplesPerPixel2d.X, NumSamplesPerPixel2d.Y),
			ComputeShader,
			PassParameters,
			GroupCount);
	}

	FRDGBufferRef ShadowMaskTileIndirectArgs = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateIndirectDesc<FRHIDispatchIndirectParameters>(1), TEXT("StochasticShadows.ShadowMaskTileIndirectArgs"));

	// Setup indirect args for shadow mask tile updates
	{
		FInitShadowMaskUpdateIndirectArgsCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FInitShadowMaskUpdateIndirectArgsCS::FParameters>();
		PassParameters->StochasticShadowsParameters = StochasticShadowsParameters;
		PassParameters->RWIndirectArgs = GraphBuilder.CreateUAV(ShadowMaskTileIndirectArgs);
		PassParameters->ShadowMaskTileAllocator = GraphBuilder.CreateSRV(ShadowMaskTileAllocator);

		auto ComputeShader = View.ShaderMap->GetShader<FInitShadowMaskUpdateIndirectArgsCS>();

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("InitShadowMaskUpdateIndirectArgs"),
			ComputeShader,
			PassParameters,
			FIntVector(1, 1, 1));
	}

	// Ray trace light samples
	if (CVarStochasticShadowsScreenTraces.GetValueOnRenderThread() != 0)
	{
		FCompactedStochasticShadowsTraceParameters CompactedTraceParameters = CompactStochasticShadowsTraces(
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
		PermutationVector.Set<FScreenSpaceRayTraceLightSamplesCS::FNumSamplesPerPixel>(NumSamplesPerPixel1d);
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

	if (CVarStochasticShadowsWorldSpaceTraces.GetValueOnRenderThread() != 0)
	{
		FCompactedStochasticShadowsTraceParameters CompactedTraceParameters = CompactStochasticShadowsTraces(
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
				PassParameters->CompactedTraceParameters = CompactedTraceParameters;
				PassParameters->StochasticShadowsParameters = StochasticShadowsParameters;
				PassParameters->RWLightSamples = GraphBuilder.CreateUAV(LightSamples);
				PassParameters->LightSampleRayDistance = LightSampleRayDistance;
				PassParameters->TLAS = View.GetRayTracingSceneLayerViewChecked(ERayTracingSceneLayer::Base);

				FHardwareRayTraceLightSamplesCS::FPermutationDomain PermutationVector;
				PermutationVector.Set<FHardwareRayTraceLightSamplesCS::FNumSamplesPerPixel>(NumSamplesPerPixel1d);
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
				PassParameters->CompactedTraceParameters = CompactedTraceParameters;
				PassParameters->StochasticShadowsParameters = StochasticShadowsParameters;
				PassParameters->RWLightSamples = GraphBuilder.CreateUAV(LightSamples);
				PassParameters->LightSampleRayDistance = LightSampleRayDistance;
				PassParameters->TLAS = View.GetRayTracingSceneLayerViewChecked(ERayTracingSceneLayer::Base);

				FHardwareRayTraceLightSamplesRGS::FPermutationDomain PermutationVector;
				PermutationVector.Set<FHardwareRayTraceLightSamplesRGS::FNumSamplesPerPixel>(NumSamplesPerPixel1d);
				PermutationVector.Set<FHardwareRayTraceLightSamplesRGS::FDebugMode>(bDebug);
				auto RayGenShader = View.ShaderMap->GetShader<FHardwareRayTraceLightSamplesRGS>(PermutationVector);

				ClearUnusedGraphResources(RayGenShader, PassParameters, { CompactedTraceParameters.IndirectArgs });

				GraphBuilder.AddPass(
					RDG_EVENT_NAME("HardwareRayTraceLightSamples RayGen"),
					PassParameters,
					ERDGPassFlags::Compute,
					[PassParameters, &View, RayGenShader, CompactedTraceTexelIndirectArgs = CompactedTraceParameters.IndirectArgs](FRHIRayTracingCommandList& RHICmdList)
					{
						CompactedTraceTexelIndirectArgs->MarkResourceAsUsed();

						FRayTracingShaderBindingsWriter GlobalResources;
						SetShaderParameters(GlobalResources, RayGenShader, *PassParameters);

						FRHIRayTracingScene* RayTracingSceneRHI = View.GetRayTracingSceneChecked();

						FRayTracingPipelineStateInitializer Initializer;

						Initializer.MaxPayloadSizeInBytes = GetRayTracingPayloadTypeMaxSize(ERayTracingPayloadType::RayTracingMaterial);

						FRHIRayTracingShader* RayGenShaderTable[] = { RayGenShader.GetRayTracingShader() };
						Initializer.SetRayGenShaderTable(RayGenShaderTable);

						FRHIRayTracingShader* HitGroupTable[] = { GetRayTracingDefaultOpaqueShader(View.ShaderMap) };
						Initializer.SetHitGroupTable(HitGroupTable);
						Initializer.bAllowHitGroupIndexing = false; // Use the same hit shader for all geometry in the scene by disabling SBT indexing.

						FRHIRayTracingShader* MissGroupTable[] = { GetRayTracingDefaultMissShader(View.ShaderMap) };
						Initializer.SetMissShaderTable(MissGroupTable);

						FRayTracingPipelineState* Pipeline = PipelineStateCache::GetAndOrCreateRayTracingPipelineState(RHICmdList, Initializer);
						RHICmdList.SetRayTracingMissShader(RayTracingSceneRHI, 0, Pipeline, 0 /* ShaderIndexInPipeline */, 0, nullptr, 0);
						RHICmdList.RayTraceDispatchIndirect(Pipeline, RayGenShader.GetRayTracingShader(), RayTracingSceneRHI, GlobalResources,
							CompactedTraceTexelIndirectArgs->GetIndirectRHICallBuffer(), (int32)StochasticShadows::ECompactedTraceIndirectArgs::NumTraces);
					}
				);
			}
#endif // RHI_RAYTRACING
		}
		else
		{
			FSoftwareRayTraceLightSamplesCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FSoftwareRayTraceLightSamplesCS::FParameters>();
			PassParameters->CompactedTraceParameters = CompactedTraceParameters;
			PassParameters->StochasticShadowsParameters = StochasticShadowsParameters;
			PassParameters->RWLightSamples = GraphBuilder.CreateUAV(LightSamples);
			PassParameters->LightSampleRayDistance = LightSampleRayDistance;

			FSoftwareRayTraceLightSamplesCS::FPermutationDomain PermutationVector;
			PermutationVector.Set<FSoftwareRayTraceLightSamplesCS::FNumSamplesPerPixel>(NumSamplesPerPixel1d);
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

	// Composite shadow masks traces
	{
		FCompositeShadowMaskTracesCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FCompositeShadowMaskTracesCS::FParameters>();
		PassParameters->StochasticShadowsParameters = StochasticShadowsParameters;
		PassParameters->IndirectArgs = ShadowMaskTileIndirectArgs;
		PassParameters->RWShadowMaskTileAtlas = GraphBuilder.CreateUAV(ShadowMaskTileAtlas);
		PassParameters->ShadowMaskPageTable = ShadowMaskPageTable;
		PassParameters->ShadowMaskTileHeader = GraphBuilder.CreateSRV(ShadowMaskTileHeader);
		PassParameters->ShadowMaskHistoryScreenCoord00 = ShadowMaskHistoryScreenCoord00;
		PassParameters->ShadowMaskHistoryWeights = ShadowMaskHistoryWeights;
		PassParameters->ShadowMaskPageTableHistory = ShadowMaskPageTableHistory;
		PassParameters->ShadowMaskHashTableHistory = ShadowMaskHashTableHistory ? GraphBuilder.CreateSRV(ShadowMaskHashTableHistory) : nullptr;
		PassParameters->ShadowMaskAtlasHistory = ShadowMaskAtlasHistory;
		PassParameters->LightSamples = LightSamples;

		FCompositeShadowMaskTracesCS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FCompositeShadowMaskTracesCS::FNumSamplesPerPixel>(NumSamplesPerPixel1d);
		PermutationVector.Set<FCompositeShadowMaskTracesCS::FTemporalAccumulation>(bTemporal);
		PermutationVector.Set<FCompositeShadowMaskTracesCS::FHashTable>(bUseHashTable);
		auto ComputeShader = View.ShaderMap->GetShader<FCompositeShadowMaskTracesCS>(PermutationVector);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("CompositeShadowMaskTraces"),
			ComputeShader,
			PassParameters,
			ShadowMaskTileIndirectArgs,
			0);
	}

	FRDGTextureRef ShadowMaskUpsampleWeights = GraphBuilder.CreateTexture(
		FRDGTextureDesc::Create2D(SceneTextures.Config.Extent, PF_FloatRGBA, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV),
		TEXT("StochasticShadows.ShadowMaskUpsampleWeights"));

	// Init shadow mask upsample weights
	{
		FInitShadowMaskUpsampleWeightsCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FInitShadowMaskUpsampleWeightsCS::FParameters>();
		PassParameters->StochasticShadowsParameters = StochasticShadowsParameters;
		PassParameters->RWShadowMaskUpsampleWeights = GraphBuilder.CreateUAV(ShadowMaskUpsampleWeights);
		PassParameters->ShadowMaskSceneDepthHistory = ShadowMaskSceneDepthHistory ? ShadowMaskSceneDepthHistory : SceneTextures.Depth.Target;
		PassParameters->HistoryScreenPositionScaleBias = HistoryScreenPositionScaleBias;
		PassParameters->HistoryUVMinMax = HistoryUVMinMax;

		auto ComputeShader = View.ShaderMap->GetShader<FInitShadowMaskUpsampleWeightsCS>();

		const FIntVector GroupCount = FComputeShaderUtils::GetGroupCount(View.ViewRect.Size(), FInitShadowMaskUpsampleWeightsCS::GetGroupSize());

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("InitShadowMaskUpsampleWeights"),
			ComputeShader,
			PassParameters,
			GroupCount);
	}

	FRDGBufferRef ShadingTileAllocator = GraphBuilder.CreateBuffer(
		FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), 1),
		TEXT("StochasticShadowsParameters.ShadingTileAllocator"));

	FRDGTextureRef ShadingTileGridAllocator = GraphBuilder.CreateTexture(
		FRDGTextureDesc::Create2D(ShadingTileGridSize, PF_R32_UINT, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV),
		TEXT("StochasticShadows.ShadingTileGridAllocator"));

	FRDGBufferRef ShadingTileGrid = GraphBuilder.CreateBuffer(
		FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), ShadingTileGridSize.X * ShadingTileGridSize.Y * StochasticShadows::MaxShadingTilesPerGridCell),
		TEXT("StochasticShadowsParameters.ShadingTileGrid"));

	FRDGTextureRef ShadingTileAtlas = GraphBuilder.CreateTexture(
		FRDGTextureDesc::Create2D(ShadingTileAtlasSize, PF_R8, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV),
		TEXT("StochasticShadows.ShadingTileAtlas"));

	AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(ShadingTileAllocator), 0u);
	AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(ShadingTileGridAllocator), 0u);

	{
		FShadowMaskSpatialPassCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FShadowMaskSpatialPassCS::FParameters>();
		PassParameters->StochasticShadowsParameters = StochasticShadowsParameters;
		PassParameters->IndirectArgs = ShadowMaskTileIndirectArgs;
		PassParameters->RWShadingTileAllocator = GraphBuilder.CreateUAV(ShadingTileAllocator);
		PassParameters->RWShadingTileGridAllocator = GraphBuilder.CreateUAV(ShadingTileGridAllocator);
		PassParameters->RWShadingTileGrid = GraphBuilder.CreateUAV(ShadingTileGrid);
		PassParameters->RWShadingTileAtlas = GraphBuilder.CreateUAV(ShadingTileAtlas);
		PassParameters->ShadowMaskUpsampleWeights = ShadowMaskUpsampleWeights;
		PassParameters->ShadowMaskTileHeader = GraphBuilder.CreateSRV(ShadowMaskTileHeader);
		PassParameters->ShadowMaskPageTable = ShadowMaskPageTable;
		PassParameters->ShadowMaskHashTable = ShadowMaskHashTable ? GraphBuilder.CreateSRV(ShadowMaskHashTable) : nullptr;
		PassParameters->ShadowMaskTileAtlas = ShadowMaskTileAtlas;

		FShadowMaskSpatialPassCS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FShadowMaskSpatialPassCS::FHashTable>(bUseHashTable);
		PermutationVector.Set<FShadowMaskSpatialPassCS::FSpatialPass>(CVarStochasticShadowsSpatial.GetValueOnRenderThread() != 0);
		auto ComputeShader = View.ShaderMap->GetShader<FShadowMaskSpatialPassCS>(PermutationVector);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("ShadowMaskSpatialPass"),
			ComputeShader,
			PassParameters,
			ShadowMaskTileIndirectArgs,
			0);
	}

	FRDGBufferRef ShadingTileIndirectArgs = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateIndirectDesc<FRHIDispatchIndirectParameters>(1), TEXT("StochasticShadows.ShadingTileIndirectArgs"));

	// Setup indirect args for shadow mask tile updates reusing FInitShadowMaskUpdateIndirectArgsCS
	{
		FInitShadowMaskUpdateIndirectArgsCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FInitShadowMaskUpdateIndirectArgsCS::FParameters>();
		PassParameters->StochasticShadowsParameters = StochasticShadowsParameters;
		PassParameters->RWIndirectArgs = GraphBuilder.CreateUAV(ShadingTileIndirectArgs);
		PassParameters->ShadowMaskTileAllocator = GraphBuilder.CreateSRV(ShadingTileAllocator);

		auto ComputeShader = View.ShaderMap->GetShader<FInitShadowMaskUpdateIndirectArgsCS>();

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("InitShadingTileIndirectArgs"),
			ComputeShader,
			PassParameters,
			FIntVector(1, 1, 1));
	}

	// Shade light samples
	{
		FShadeLightSamplesWithShadowMaskCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FShadeLightSamplesWithShadowMaskCS::FParameters>();
		PassParameters->StochasticShadowsParameters = StochasticShadowsParameters;
		PassParameters->ShadowMaskPageTable = ShadowMaskPageTable;
		PassParameters->ShadowMaskTileAtlas = ShadowMaskTileAtlas;
		PassParameters->ShadowMaskTileAllocator = GraphBuilder.CreateSRV(ShadowMaskTileAllocator);
		PassParameters->ShadingTileAllocator = GraphBuilder.CreateSRV(ShadingTileAllocator);
		PassParameters->ShadingTileGridAllocator = ShadingTileGridAllocator;
		PassParameters->ShadingTileGrid = GraphBuilder.CreateSRV(ShadingTileGrid);
		PassParameters->ShadingTileAtlas = ShadingTileAtlas;
		PassParameters->RWSceneColor = GraphBuilder.CreateUAV(SceneTextures.Color.Target);
		PassParameters->ShadowMaskHashTable = ShadowMaskHashTable ? GraphBuilder.CreateSRV(ShadowMaskHashTable) : nullptr;

		FShadeLightSamplesWithShadowMaskCS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FShadeLightSamplesWithShadowMaskCS::FDebugMode>(bDebug);
		auto ComputeShader = View.ShaderMap->GetShader<FShadeLightSamplesWithShadowMaskCS>(PermutationVector);

		const FIntVector GroupCount = FComputeShaderUtils::GetGroupCount(View.ViewRect.Size(), FShadeLightSamplesWithShadowMaskCS::GetGroupSize());

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("ShadeLightSamplesWithShadowMask"),
			ComputeShader,
			PassParameters,
			GroupCount);
	}

	if (View.ViewState && !View.bStatePrevViewInfoIsReadOnly)
	{
		FStochasticShadowsViewState& LightingViewState = View.ViewState->StochasticShadows;
		LightingViewState.ShadowMaskPageTableHistory = nullptr;
		LightingViewState.ShadowMaskHashTableHistory = nullptr;
		LightingViewState.ShadowMaskAtlasHistory = nullptr;
		LightingViewState.ShadowMaskSceneDepthHistory = nullptr;

		if (ShadowMaskHashTable)
		{
			GraphBuilder.QueueBufferExtraction(ShadowMaskHashTable, &LightingViewState.ShadowMaskHashTableHistory);
		}

		if (ShadowMaskPageTable)
		{
			GraphBuilder.QueueTextureExtraction(ShadowMaskPageTable, &LightingViewState.ShadowMaskPageTableHistory);
		}

		if (ShadowMaskTileAtlas)
		{
			GraphBuilder.QueueTextureExtraction(ShadowMaskTileAtlas, &LightingViewState.ShadowMaskAtlasHistory);
		}

		if (DownsampledSceneDepth)
		{
			GraphBuilder.QueueTextureExtraction(DownsampledSceneDepth, &LightingViewState.ShadowMaskSceneDepthHistory);
		}

		LightingViewState.HistoryScreenPositionScaleBias = View.GetScreenPositionScaleBias(View.GetSceneTexturesConfig().Extent, View.ViewRect);

		// Pull in the max UV to exclude the region which will read outside the viewport due to bilinear filtering
		const FVector2D InvBufferSize(1.0f / SceneTextures.Config.Extent.X, 1.0f / SceneTextures.Config.Extent.Y);
		LightingViewState.HistoryUVMinMax = FVector4f(
			(View.ViewRect.Min.X + 0.5f) * InvBufferSize.X,
			(View.ViewRect.Min.Y + 0.5f) * InvBufferSize.Y,
			(View.ViewRect.Max.X - 1.0f) * InvBufferSize.X,
			(View.ViewRect.Max.Y - 1.0f) * InvBufferSize.Y);
	}
}