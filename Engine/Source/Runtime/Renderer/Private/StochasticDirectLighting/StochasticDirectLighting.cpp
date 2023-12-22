// Copyright Epic Games, Inc. All Rights Reserved.

#include "StochasticDirectLighting.h"
#include "StochasticDirectLightingInternal.h"
#include "RendererPrivate.h"
#include "PixelShaderUtils.h"
#include "BasePassRendering.h"

static TAutoConsoleVariable<int32> CVarStochasticDirectLighting(
	TEXT("r.StochasticDirectLighting"),
	0,
	TEXT("Whether to enable Stochastic Direct Lighting. Experimental feature stochastically sampling analytical light in a single pass by leveraging ray tracing.\n")
	TEXT("1 - all lights using ray tracing shadows will be stochastically sampled\n")
	TEXT("2 - all lights will be stochastically sampled"),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarStochasticDirectLightingNumSamplesPerPixel(
	TEXT("r.StochasticDirectLighting.NumSamplesPerPixel"),
	4,
	TEXT("Number of samples (shadow rays) per half-res pixel.\n")
	TEXT("1 - 0.25 trace per pixel\n")
	TEXT("2 - 0.5 trace per pixel\n")
	TEXT("4 - 1 trace per pixel"),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarStochasticDirectLightingMaxShadingTilesPerGridCell(
	TEXT("r.StochasticDirectLighting.MaxShadingTilesPerGridCell"),
	32,
	TEXT("Maximum number of shading tiles per grid cell."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> CVarStochasticDirectLightingSamplingMinWeight(
	TEXT("r.StochasticDirectLighting.Sampling.MinWeight"),
	0.001f,
	TEXT("Determines minimal sample influence on final pixels. Used to skip samples which would have minimal impact to the final image even if light is fully visible."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarStochasticDirectLightingTemporal(
	TEXT("r.StochasticDirectLighting.Temporal"),
	1,
	TEXT("Whether to use temporal accumulation for shadow mask."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarStochasticDirectLightingTemporalMaxFramesAccumulated(
	TEXT("r.StochasticDirectLighting.Temporal.MaxFramesAccumulated"),
	8,
	TEXT("Max history length when accumulating frames. Lower values have less ghosting, but more noise."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> CVarStochasticDirectLightingTemporalNeighborhoodClampScale(
	TEXT("r.StochasticDirectLighting.Temporal.NeighborhoodClampScale"),
	2.0f,
	TEXT("Scales how permissive is neighborhood clamp. Higher values cause more ghosting, but allow smoother temporal accumulation."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarStochasticDirectLightingSpatial(
	TEXT("r.StochasticDirectLighting.Spatial"),
	1,
	TEXT("Whether denoiser should run spatial filter."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> CVarStochasticDirectLightingSpatialDepthWeightScale(
	TEXT("r.StochasticDirectLighting.Spatial.DepthWeightScale"),
	10000.0f,
	TEXT("Scales the depth weight of the spatial filter. Smaller values allow for more sample reuse, but also introduce more bluriness between unrelated surfaces."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarStochasticDirectLightingWaveOps(
	TEXT("r.StochasticDirectLighting.WaveOps"),
	1,
	TEXT("Whether to use wave ops. Useful for debugging."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarStochasticDirectLightingDebug(
	TEXT("r.StochasticDirectLighting.Debug"),
	0,
	TEXT("Whether to enabled debug mode, which prints various extra debug information from shaders.")
	TEXT("0 - Disable\n")
	TEXT("1 - Visualize sampling\n")
	TEXT("2 - Visualize tracing\n"),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarStochasticDirectLightingDebugLightId(
	TEXT("r.StochasticDirectLighting.Debug.LightId"),
	-1,
	TEXT("Which light to show debug info for. When set to -1, uses the currently selected light in editor."),
	ECVF_RenderThreadSafe
);

int32 GStochasticDirectLightingReset = 0;
FAutoConsoleVariableRef CVarStochasticDirectLightingReset(
	TEXT("r.StochasticDirectLighting.Reset"),
	GStochasticDirectLightingReset,
	TEXT("Reset history for debugging."),
	ECVF_RenderThreadSafe
);

int32 GStochasticDirectLightingResetEveryNthFrame = 0;
	FAutoConsoleVariableRef CVarStochasticDirectLightingResetEveryNthFrame(
	TEXT("r.StochasticDirectLighting.ResetEveryNthFrame"),
		GStochasticDirectLightingResetEveryNthFrame,
	TEXT("Reset history every Nth frame for debugging."),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int> CVarStochasticDirectLightingFixedStateFrameIndex(
	TEXT("r.StochasticDirectLighting.FixedStateFrameIndex"),
	-1,
	TEXT("Whether to override View.StateFrameIndex for debugging."),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int> CVarStochasticDirectLightingCandidateLightMask(
	TEXT("r.StochasticDirectLighting.CandidateLightMask"),
	1,
	TEXT("#sdl_todo: finish and pick one shader path."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int> CVarStochasticDirectLightingTexturedRectLights(
	TEXT("r.StochasticDirectLighting.TexturedRectLights"),
	0,
	TEXT("Whether to support textured rect lights."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int> CVarStochasticDirectLightingLightFunctions(
	TEXT("r.StochasticDirectLighting.LightFunctions"),
	0,
	TEXT("Whether to support light functions."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int> CVarStochasticDirectLightingIESProfiles(
	TEXT("r.StochasticDirectLighting.IESProfiles"),
	1,
	TEXT("Whether to support IES profiles on lights."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

namespace StochasticDirectLighting
{
	// must match values in StochasticDirectLighting.ush
	constexpr int32 TileSize = 8;
	constexpr int32 MaxLocalLightIndexXY = 16; // 16 * 16 = 256
	constexpr uint32 ShadingTileIndexUnshadowed = 0xFFFFF; // limited by PackShadingTile()
	constexpr uint32 ShadingAtlasSizeInTiles = 512;

	bool IsEnabled()
	{
		return CVarStochasticDirectLighting.GetValueOnRenderThread() != 0;
	}

	bool IsUsingLightFunctions()
	{
		return IsEnabled() && CVarStochasticDirectLightingLightFunctions.GetValueOnRenderThread() != 0;
	}

	bool IsLightSupported(uint8 LightType, ECastRayTracedShadow::Type CastRayTracedShadow)
	{
		if (StochasticDirectLighting::IsEnabled() && LightType != LightType_Directional)
		{
			const bool bRayTracedShadows = (CastRayTracedShadow == ECastRayTracedShadow::Enabled || (ShouldRenderRayTracingShadows() && CastRayTracedShadow == ECastRayTracedShadow::UseProjectSetting));
			return CVarStochasticDirectLighting.GetValueOnRenderThread() == 2 || bRayTracedShadows;
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

	uint32 GetStateFrameIndex(FSceneViewState* ViewState)
	{
		uint32 StateFrameIndex = ViewState ? ViewState->GetFrameIndex() : 0;

		if (CVarStochasticDirectLightingFixedStateFrameIndex.GetValueOnRenderThread() >= 0)
		{
			StateFrameIndex = CVarStochasticDirectLightingFixedStateFrameIndex.GetValueOnRenderThread();
		}

		return StateFrameIndex;
	}

	FIntPoint GetNumSamplesPerPixel2d()
	{
		const uint32 NumSamplesPerPixel1d = FMath::RoundUpToPowerOfTwo(FMath::Clamp(CVarStochasticDirectLightingNumSamplesPerPixel.GetValueOnRenderThread(), 1, 4));
		return NumSamplesPerPixel1d == 4 ? FIntPoint(2, 2) : (NumSamplesPerPixel1d == 2 ? FIntPoint(2, 1) : FIntPoint(1, 1));
	}

	int32 GetDebugMode()
	{
		return CVarStochasticDirectLightingDebug.GetValueOnRenderThread();
	}

	bool UseWaveOps(EShaderPlatform ShaderPlatform)
	{
		return CVarStochasticDirectLightingWaveOps.GetValueOnRenderThread() != 0
			&& GRHISupportsWaveOperations
			&& RHISupportsWaveOperations(ShaderPlatform);
	}

	void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FForwardLightingParameters::ModifyCompilationEnvironment(Platform, OutEnvironment);
	}

	// Keep in sync with TILE_TYPE_* in shaders
	enum class ETileType : uint8
	{
		SimpleShading = 0,
		ComplexShading = 1,
		SHADING_MAX = 2,

		Empty = 2,
		MAX = 3
	};
};

class FTileClassificationCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FTileClassificationCS)
	SHADER_USE_PARAMETER_STRUCT(FTileClassificationCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FStochasticDirectLightingParameters, StochasticDirectLightingParameters)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, RWTileAllocator)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, RWTileData)
	END_SHADER_PARAMETER_STRUCT()

	class FDownsampledClassification : SHADER_PERMUTATION_BOOL("DOWNSAMPLED_CLASSIFICATION");
	using FPermutationDomain = TShaderPermutationDomain<FDownsampledClassification>;

	static int32 GetGroupSize()
	{	
		return 8;
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return StochasticDirectLighting::ShouldCompileShaders(Parameters);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());
	}
};

IMPLEMENT_GLOBAL_SHADER(FTileClassificationCS, "/Engine/Private/StochasticDirectLighting/StochasticDirectLighting.usf", "TileClassificationCS", SF_Compute);

class FInitTileIndirectArgsCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FInitTileIndirectArgsCS)
	SHADER_USE_PARAMETER_STRUCT(FInitTileIndirectArgsCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FStochasticDirectLightingParameters, StochasticDirectLightingParameters)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWTileIndirectArgs)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWDownsampledTileIndirectArgs)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, TileAllocator)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, DownsampledTileAllocator)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return StochasticDirectLighting::ShouldCompileShaders(Parameters);
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

IMPLEMENT_GLOBAL_SHADER(FInitTileIndirectArgsCS, "/Engine/Private/StochasticDirectLighting/StochasticDirectLighting.usf", "InitTileIndirectArgsCS", SF_Compute);

class FInitCompositeIndirectArgsCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FInitCompositeIndirectArgsCS)
	SHADER_USE_PARAMETER_STRUCT(FInitCompositeIndirectArgsCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FStochasticDirectLightingParameters, StochasticDirectLightingParameters)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWIndirectArgs)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, CompositeTileAllocator)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return StochasticDirectLighting::ShouldCompileShaders(Parameters);
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

IMPLEMENT_GLOBAL_SHADER(FInitCompositeIndirectArgsCS, "/Engine/Private/StochasticDirectLighting/StochasticDirectLighting.usf", "InitCompositeIndirectArgsCS", SF_Compute);

class FGenerateSamplesCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FGenerateSamplesCS)
	SHADER_USE_PARAMETER_STRUCT(FGenerateSamplesCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		RDG_BUFFER_ACCESS(IndirectArgs, ERHIAccess::IndirectArgs)
		SHADER_PARAMETER_STRUCT_INCLUDE(FStochasticDirectLightingParameters, StochasticDirectLightingParameters)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float>, RWDownsampledSceneDepth)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<UNORM float3>, RWDownsampledSceneWorldNormal)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, RWCompositeTileAllocator)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint2>, RWCompositeTileData)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<uint>, RWLightSamples)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, DownsampledTileAllocator)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, DownsampledTileData)
		SHADER_PARAMETER(FVector4f, HistoryUVMinMax)
		SHADER_PARAMETER(FVector4f, HistoryScreenPositionScaleBias)
	END_SHADER_PARAMETER_STRUCT()

	class FTileType : SHADER_PERMUTATION_INT("TILE_TYPE", (int32)StochasticDirectLighting::ETileType::SHADING_MAX);
	class FIESProfile : SHADER_PERMUTATION_BOOL("USE_IES_PROFILE");
	class FLightFunctionAtlas : SHADER_PERMUTATION_BOOL("USE_LIGHT_FUNCTION_ATLAS");
	class FTexturedRectLights : SHADER_PERMUTATION_BOOL("USE_SOURCE_TEXTURE");
	class FNumSamplesPerPixel1d : SHADER_PERMUTATION_SPARSE_INT("NUM_SAMPLES_PER_PIXEL_1D", 1, 2, 4);
	class FCandidateLightMask : SHADER_PERMUTATION_BOOL("CANDIDATE_LIGHT_MASK");
	class FDebugMode : SHADER_PERMUTATION_BOOL("DEBUG_MODE");
	using FPermutationDomain = TShaderPermutationDomain<FTileType, FIESProfile, FLightFunctionAtlas, FTexturedRectLights, FNumSamplesPerPixel1d, FCandidateLightMask, FDebugMode>;

	static int32 GetGroupSize()
	{	
		return 8;
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return StochasticDirectLighting::ShouldCompileShaders(Parameters);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		StochasticDirectLighting::ModifyCompilationEnvironment(Parameters.Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());
	}
};

IMPLEMENT_GLOBAL_SHADER(FGenerateSamplesCS, "/Engine/Private/StochasticDirectLighting/StochasticDirectLightingSampling.usf", "GenerateSamplesCS", SF_Compute);

class FClearLightSamplesCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FClearLightSamplesCS)
	SHADER_USE_PARAMETER_STRUCT(FClearLightSamplesCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		RDG_BUFFER_ACCESS(IndirectArgs, ERHIAccess::IndirectArgs)
		SHADER_PARAMETER_STRUCT_INCLUDE(FStochasticDirectLightingParameters, StochasticDirectLightingParameters)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float>, RWDownsampledSceneDepth)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<UNORM float3>, RWDownsampledSceneWorldNormal)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<uint>, RWLightSamples)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, DownsampledTileAllocator)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, DownsampledTileData)
	END_SHADER_PARAMETER_STRUCT()

	class FDebugMode : SHADER_PERMUTATION_BOOL("DEBUG_MODE");
	using FPermutationDomain = TShaderPermutationDomain<FDebugMode>;

	static int32 GetGroupSize()
	{
		return 8;
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return StochasticDirectLighting::ShouldCompileShaders(Parameters);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		StochasticDirectLighting::ModifyCompilationEnvironment(Parameters.Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());
	}
};

IMPLEMENT_GLOBAL_SHADER(FClearLightSamplesCS, "/Engine/Private/StochasticDirectLighting/StochasticDirectLightingSampling.usf", "ClearLightSamplesCS", SF_Compute);

class FInitCompositeUpsampleWeightsCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FInitCompositeUpsampleWeightsCS)
	SHADER_USE_PARAMETER_STRUCT(FInitCompositeUpsampleWeightsCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FStochasticDirectLightingParameters, StochasticDirectLightingParameters)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, RWCompositeUpsampleWeights)
	END_SHADER_PARAMETER_STRUCT()

	static int32 GetGroupSize()
	{
		return 8;
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return StochasticDirectLighting::ShouldCompileShaders(Parameters);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());
	}
};

IMPLEMENT_GLOBAL_SHADER(FInitCompositeUpsampleWeightsCS, "/Engine/Private/StochasticDirectLighting/StochasticDirectLighting.usf", "InitCompositeUpsampleWeightsCS", SF_Compute);

class FCompositeLightSamplesCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FCompositeLightSamplesCS)
	SHADER_USE_PARAMETER_STRUCT(FCompositeLightSamplesCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FStochasticDirectLightingParameters, StochasticDirectLightingParameters)
		RDG_BUFFER_ACCESS(IndirectArgs, ERHIAccess::IndirectArgs)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, RWShadingTileAllocator)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<uint>, RWShadingTileGridAllocator)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, RWShadingTileGrid)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float>, RWShadingTileAtlas)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint2>, CompositeTileData)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<flaot4>, CompositeUpsampleWeights)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<uint>, LightSamples)
	END_SHADER_PARAMETER_STRUCT()

	static int32 GetGroupSize()
	{
		return 8;
	}

	class FDebugMode : SHADER_PERMUTATION_BOOL("DEBUG_MODE");
	using FPermutationDomain = TShaderPermutationDomain<FDebugMode>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return StochasticDirectLighting::ShouldCompileShaders(Parameters);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		StochasticDirectLighting::ModifyCompilationEnvironment(Parameters.Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());
	}
};

IMPLEMENT_GLOBAL_SHADER(FCompositeLightSamplesCS, "/Engine/Private/StochasticDirectLighting/StochasticDirectLightingComposite.usf", "CompositeLightSamplesCS", SF_Compute);

class FShadeLightSamplesCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FShadeLightSamplesCS)
	SHADER_USE_PARAMETER_STRUCT(FShadeLightSamplesCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		RDG_BUFFER_ACCESS(IndirectArgs, ERHIAccess::IndirectArgs)
		SHADER_PARAMETER_STRUCT_INCLUDE(FStochasticDirectLightingParameters, StochasticDirectLightingParameters)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float3>, RWResolvedDiffuseLighting)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float3>, RWResolvedSpecularLighting)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, TileAllocator)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, TileData)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, ShadingTileAllocator)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, CompositeTileAllocator)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<uint>, ShadingTileGridAllocator)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, ShadingTileGrid)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, ShadingTiles)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float>, ShadingTileAtlas)
	END_SHADER_PARAMETER_STRUCT()

	static int32 GetGroupSize()
	{
		return 8;
	}

	class FTileType : SHADER_PERMUTATION_INT("TILE_TYPE", (int32)StochasticDirectLighting::ETileType::SHADING_MAX);
	class FIESProfile : SHADER_PERMUTATION_BOOL("USE_IES_PROFILE");
	class FLightFunctionAtlas : SHADER_PERMUTATION_BOOL("USE_LIGHT_FUNCTION_ATLAS");
	class FTexturedRectLights : SHADER_PERMUTATION_BOOL("USE_SOURCE_TEXTURE");
	class FDebugMode : SHADER_PERMUTATION_BOOL("DEBUG_MODE");
	using FPermutationDomain = TShaderPermutationDomain<FTileType, FIESProfile, FLightFunctionAtlas, FTexturedRectLights, FDebugMode>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return StochasticDirectLighting::ShouldCompileShaders(Parameters);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		StochasticDirectLighting::ModifyCompilationEnvironment(Parameters.Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());
	}
};

IMPLEMENT_GLOBAL_SHADER(FShadeLightSamplesCS, "/Engine/Private/StochasticDirectLighting/StochasticDirectLightingShading.usf", "ShadeLightSamplesCS", SF_Compute);

class FSDLTemporalAccumulationCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FSDLTemporalAccumulationCS)
	SHADER_USE_PARAMETER_STRUCT(FSDLTemporalAccumulationCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FStochasticDirectLightingParameters, StochasticDirectLightingParameters)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float4>, ResolvedDiffuseLighting)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float4>, ResolvedSpecularLighting)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float4>, DiffuseLightingAndSecondMomentHistoryTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float4>, SpecularLightingAndSecondMomentHistoryTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<UNORM float>, NumFramesAccumulatedHistoryTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float>, StochasticDirectLightingDepthHistory)
		SHADER_PARAMETER(FVector4f, HistoryUVMinMax)
		SHADER_PARAMETER(FVector4f, HistoryScreenPositionScaleBias)
		SHADER_PARAMETER(float, PrevSceneColorPreExposureCorrection)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, RWDiffuseLightingAndSecondMoment)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, RWSpecularLightingAndSecondMoment)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<UNORM float>, RWNumFramesAccumulated)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float>, RWSceneDepth)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, RWSceneColor)
	END_SHADER_PARAMETER_STRUCT()

	class FValidHistory : SHADER_PERMUTATION_BOOL("VALID_HISTORY");
	class FDebugMode : SHADER_PERMUTATION_BOOL("DEBUG_MODE");
	using FPermutationDomain = TShaderPermutationDomain<FValidHistory, FDebugMode>;

	static int32 GetGroupSize()
	{
		return 8;
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return StochasticDirectLighting::ShouldCompileShaders(Parameters);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());
	}
};

IMPLEMENT_GLOBAL_SHADER(FSDLTemporalAccumulationCS, "/Engine/Private/StochasticDirectLighting/StochasticDirectLightingTemporal.usf", "SDLTemporalAccumulationCS", SF_Compute);

class FSDLSpatialFilterCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FSDLSpatialFilterCS)
	SHADER_USE_PARAMETER_STRUCT(FSDLSpatialFilterCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FStochasticDirectLightingParameters, StochasticDirectLightingParameters)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, RWSceneColor)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float3>, DiffuseLightingAndSecondMomentTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float3>, SpecularLightingAndSecondMomentTexture)
		SHADER_PARAMETER(float, SpatialFilterDepthWeightScale)
	END_SHADER_PARAMETER_STRUCT()

	class FSpatialFilter : SHADER_PERMUTATION_BOOL("SPATIAL_FILTER");
	class FDebugMode : SHADER_PERMUTATION_BOOL("DEBUG_MODE");
	using FPermutationDomain = TShaderPermutationDomain<FSpatialFilter, FDebugMode>;

	static int32 GetGroupSize()
	{
		return 8;
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return StochasticDirectLighting::ShouldCompileShaders(Parameters);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());
	}
};

IMPLEMENT_GLOBAL_SHADER(FSDLSpatialFilterCS, "/Engine/Private/StochasticDirectLighting/StochasticDirectLightingSpatial.usf", "SDLSpatialFilterCS", SF_Compute);

/**
 * Single pass batched light rendering using ray tracing (distance field or triangle) for shadowing.
 */
void FDeferredShadingSceneRenderer::RenderStochasticDirectLighting(FRDGBuilder& GraphBuilder, const FSceneTextures& SceneTextures)
{
	if (!StochasticDirectLighting::IsEnabled())
	{
		return;
	}

	check(AreLightsInLightGrid());

	RDG_EVENT_SCOPE(GraphBuilder, "StochasticDirectLighting");

	const uint32 ViewIndex = 0;
	const FViewInfo& View = Views[ViewIndex];
	FBlueNoise BlueNoise = GetBlueNoiseGlobalParameters();
	TUniformBufferRef<FBlueNoise> BlueNoiseUniformBuffer = CreateUniformBufferImmediate(BlueNoise, EUniformBufferUsage::UniformBuffer_SingleDraw);

	const bool bDebug = StochasticDirectLighting::GetDebugMode() != 0;
	const bool bWaveOps = StochasticDirectLighting::UseWaveOps(View.GetShaderPlatform())
		&& GRHIMinimumWaveSize <= 32
		&& GRHIMaximumWaveSize >= 32;

	// History reset for debugging purposes
	bool bResetHistory = false;

	if (GStochasticDirectLightingResetEveryNthFrame > 0 && (ViewFamily.FrameNumber % (uint32)GStochasticDirectLightingResetEveryNthFrame) == 0)
	{
		bResetHistory = true;
	}

	if (GStochasticDirectLightingReset != 0)
	{
		GStochasticDirectLightingReset = 0;
		bResetHistory = true;
	}

	const FIntPoint NumSamplesPerPixel2d = StochasticDirectLighting::GetNumSamplesPerPixel2d();

	const uint32 DownsampleFactor = 2;
	const FIntPoint DownsampledViewSize = FIntPoint::DivideAndRoundUp(View.ViewRect.Size(), DownsampleFactor);
	const FIntPoint SampleViewSize = DownsampledViewSize * NumSamplesPerPixel2d;
	const FIntPoint DownsampledBufferSize = FIntPoint::DivideAndRoundUp(SceneTextures.Config.Extent, DownsampleFactor);
	const FIntPoint SampleBufferSize = DownsampledBufferSize * NumSamplesPerPixel2d;
	const FIntPoint DonwnsampledSampleBufferSize = DownsampledBufferSize * NumSamplesPerPixel2d;

	// #sdl_todo: make atlas size based on the resolution
	const FIntPoint ShadingTileGridSize = FIntPoint::DivideAndRoundUp(SceneTextures.Config.Extent, StochasticDirectLighting::TileSize);
	const FIntPoint ShadingTileAtlasSize = StochasticDirectLighting::TileSize * StochasticDirectLighting::ShadingAtlasSizeInTiles;
	const int32 MaxShadingTiles = StochasticDirectLighting::ShadingAtlasSizeInTiles * StochasticDirectLighting::ShadingAtlasSizeInTiles;
	check(MaxShadingTiles == (ShadingTileAtlasSize.X * ShadingTileAtlasSize.Y) / (StochasticDirectLighting::TileSize * StochasticDirectLighting::TileSize));
	check(MaxShadingTiles < StochasticDirectLighting::ShadingTileIndexUnshadowed);
	const int32 MaxCompositeTiles = MaxShadingTiles;

	const int32 MaxShadingTilesPerGridCell = FMath::Max(CVarStochasticDirectLightingMaxShadingTilesPerGridCell.GetValueOnRenderThread(), 0);

	FRDGTextureRef DownsampledSceneDepth = GraphBuilder.CreateTexture(
		FRDGTextureDesc::Create2D(DownsampledBufferSize, PF_R32_FLOAT, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV),
		TEXT("StochasticDirectLighting.DownsampledSceneDepth"));

	FRDGTextureRef DownsampledSceneWorldNormal = GraphBuilder.CreateTexture(
		FRDGTextureDesc::Create2D(DownsampledBufferSize, PF_A2B10G10R10, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV),
		TEXT("StochasticDirectLighting.DownsampledSceneWorldNormal"));

	FRDGTextureRef LightSamples = GraphBuilder.CreateTexture(
		FRDGTextureDesc::Create2D(DonwnsampledSampleBufferSize, PF_R32_UINT, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV),
		TEXT("StochasticDirectLighting.LightSamples"));

	FRDGTextureRef LightSampleRayDistance = GraphBuilder.CreateTexture(
		FRDGTextureDesc::Create2D(DonwnsampledSampleBufferSize, PF_R16F, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV),
		TEXT("StochasticDirectLighting.LightSampleRayDistance"));
	
	bool bTemporal = CVarStochasticDirectLightingTemporal.GetValueOnRenderThread() != 0;
	FVector4f HistoryScreenPositionScaleBias = FVector4f(0.0f, 0.0f, 0.0f, 0.0f);
	FVector4f HistoryUVMinMax = FVector4f(0.0f, 0.0f, 0.0f, 0.0f);
	FRDGTextureRef DiffuseLightingAndSecondMomentHistory = nullptr;
	FRDGTextureRef SpecularLightingAndSecondMomentHistory = nullptr;
	FRDGTextureRef SceneDepthHistory = nullptr;
	FRDGTextureRef NumFramesAccumulatedHistory = nullptr;

	if (View.ViewState)
	{
		const FStochasticDirectLightingViewState& LightingViewState = View.ViewState->StochasticDirectLighting;

		if (!View.bCameraCut && !bResetHistory && bTemporal)
		{
			HistoryScreenPositionScaleBias = LightingViewState.HistoryScreenPositionScaleBias;
			HistoryUVMinMax = LightingViewState.HistoryUVMinMax;

			if (LightingViewState.DiffuseLightingAndSecondMomentHistory
				&& LightingViewState.SpecularLightingAndSecondMomentHistory
				&& LightingViewState.SceneDepthHistory
				&& LightingViewState.NumFramesAccumulatedHistory
				&& LightingViewState.DiffuseLightingAndSecondMomentHistory->GetDesc().Extent == View.GetSceneTexturesConfig().Extent
				&& LightingViewState.SpecularLightingAndSecondMomentHistory->GetDesc().Extent == View.GetSceneTexturesConfig().Extent
				&& LightingViewState.SceneDepthHistory->GetDesc().Extent == SceneTextures.Depth.Resolve->Desc.Extent)
			{
				DiffuseLightingAndSecondMomentHistory = GraphBuilder.RegisterExternalTexture(LightingViewState.DiffuseLightingAndSecondMomentHistory);
				SpecularLightingAndSecondMomentHistory = GraphBuilder.RegisterExternalTexture(LightingViewState.SpecularLightingAndSecondMomentHistory);
				SceneDepthHistory = GraphBuilder.RegisterExternalTexture(LightingViewState.SceneDepthHistory);
				NumFramesAccumulatedHistory = GraphBuilder.RegisterExternalTexture(LightingViewState.NumFramesAccumulatedHistory);
			}
		}
	}

	// Setup the light function atlas
	const bool bUseLightFunctionAtlas = LightFunctionAtlas::IsEnabled(View, LightFunctionAtlas::ELightFunctionAtlasSystem::StochasticDirectLighting);

	const FIntPoint ViewSizeInTiles = FIntPoint::DivideAndRoundUp(View.ViewRect.Size(), StochasticDirectLighting::TileSize);
	const int32 TileDataStride = ViewSizeInTiles.X * ViewSizeInTiles.Y;

	const FIntPoint DownsampledViewSizeInTiles = FIntPoint::DivideAndRoundUp(DownsampledViewSize, StochasticDirectLighting::TileSize);
	const int32 DownsampledTileDataStride = DownsampledViewSizeInTiles.X * DownsampledViewSizeInTiles.Y;

	FRDGTextureRef DownsampledTileMask = GraphBuilder.CreateTexture(
		FRDGTextureDesc::Create2D(FMath::DivideAndRoundUp<FIntPoint>(DownsampledBufferSize, StochasticDirectLighting::TileSize), PF_R8_UINT, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV),
		TEXT("StochasticDirectLighting.DownsampledTileMask"));

	FStochasticDirectLightingParameters StochasticDirectLightingParameters;
	{
		StochasticDirectLightingParameters.ViewUniformBuffer = View.ViewUniformBuffer;
		StochasticDirectLightingParameters.Scene = View.GetSceneUniforms().GetBuffer(GraphBuilder);
		StochasticDirectLightingParameters.SceneTextures = GetSceneTextureParameters(GraphBuilder, SceneTextures.UniformBuffer);
		StochasticDirectLightingParameters.SceneTexturesStruct = SceneTextures.UniformBuffer;
		StochasticDirectLightingParameters.Substrate = Substrate::BindSubstrateGlobalUniformParameters(View);
		StochasticDirectLightingParameters.ForwardLightData = View.ForwardLightingResources.ForwardLightUniformBuffer;
		StochasticDirectLightingParameters.LightFunctionAtlas = LightFunctionAtlas::BindGlobalParameters(GraphBuilder, View);
		StochasticDirectLightingParameters.BlueNoise = BlueNoiseUniformBuffer;
		StochasticDirectLightingParameters.PreIntegratedGF = GSystemTextures.PreintegratedGF->GetRHI();
		StochasticDirectLightingParameters.PreIntegratedGFSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
		StochasticDirectLightingParameters.DownsampledViewSize = DownsampledViewSize;
		StochasticDirectLightingParameters.SampleViewSize = SampleViewSize;
		StochasticDirectLightingParameters.NumSamplesPerPixel = NumSamplesPerPixel2d;
		StochasticDirectLightingParameters.NumSamplesPerPixelDivideShift.X = FMath::FloorLog2(NumSamplesPerPixel2d.X);
		StochasticDirectLightingParameters.NumSamplesPerPixelDivideShift.Y = FMath::FloorLog2(NumSamplesPerPixel2d.Y);
		StochasticDirectLightingParameters.StochasticDirectLightingStateFrameIndex = StochasticDirectLighting::GetStateFrameIndex(View.ViewState);
		StochasticDirectLightingParameters.DownsampledTileMask = DownsampledTileMask;
		StochasticDirectLightingParameters.DownsampledSceneDepth = DownsampledSceneDepth;
		StochasticDirectLightingParameters.DownsampledSceneWorldNormal = DownsampledSceneWorldNormal;
		StochasticDirectLightingParameters.MaxCompositeTiles = MaxCompositeTiles;
		StochasticDirectLightingParameters.MaxShadingTiles = MaxShadingTiles;
		StochasticDirectLightingParameters.MaxShadingTilesPerGridCell = MaxShadingTilesPerGridCell;
		StochasticDirectLightingParameters.ShadingTileGridSize = ShadingTileGridSize;
		StochasticDirectLightingParameters.DownsampledBufferInvSize = FVector2f(1.0f) / DownsampledBufferSize;
		StochasticDirectLightingParameters.SamplingMinWeight = FMath::Max(CVarStochasticDirectLightingSamplingMinWeight.GetValueOnRenderThread(), 0.0f);
		StochasticDirectLightingParameters.TileDataStride = TileDataStride;
		StochasticDirectLightingParameters.DownsampledTileDataStride = DownsampledTileDataStride;
		StochasticDirectLightingParameters.TemporalMaxFramesAccumulated = FMath::Max(CVarStochasticDirectLightingTemporalMaxFramesAccumulated.GetValueOnRenderThread(), 0.0f);
		StochasticDirectLightingParameters.TemporalNeighborhoodClampScale = CVarStochasticDirectLightingTemporalNeighborhoodClampScale.GetValueOnRenderThread();
		StochasticDirectLightingParameters.DebugMode = StochasticDirectLighting::GetDebugMode();
		StochasticDirectLightingParameters.DebugLightId = INDEX_NONE;

		if (bDebug)
		{
			ShaderPrint::SetEnabled(true);
			ShaderPrint::RequestSpaceForLines(1024);
			ShaderPrint::SetParameters(GraphBuilder, View.ShaderPrintData, StochasticDirectLightingParameters.ShaderPrintUniformBuffer);

			StochasticDirectLightingParameters.DebugLightId = CVarStochasticDirectLightingDebugLightId.GetValueOnRenderThread();

			if (StochasticDirectLightingParameters.DebugLightId < 0)
			{
				for (auto LightIt = Scene->Lights.CreateConstIterator(); LightIt; ++LightIt)
				{
					const FLightSceneInfoCompact& LightSceneInfoCompact = *LightIt;
					const FLightSceneInfo* const LightSceneInfo = LightSceneInfoCompact.LightSceneInfo;

					if (LightSceneInfo->Proxy->IsSelected())
					{
						StochasticDirectLightingParameters.DebugLightId = LightSceneInfo->Id;
						break;
					}
				}
			}
		}
	}

	FRDGBufferRef TileAllocator = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), (int32)StochasticDirectLighting::ETileType::MAX), TEXT("StochasticDirectLighting.TileAllocator"));
	FRDGBufferRef TileData = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), TileDataStride * (int32)StochasticDirectLighting::ETileType::MAX), TEXT("StochasticDirectLighting.TileData"));
	AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(TileAllocator), 0);

	FRDGBufferRef DownsampledTileAllocator = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), (int32)StochasticDirectLighting::ETileType::MAX), TEXT("StochasticDirectLighting.DownsampledTileAllocator"));
	FRDGBufferRef DownsampledTileData = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), DownsampledTileDataStride * (int32)StochasticDirectLighting::ETileType::MAX), TEXT("StochasticDirectLighting.DownsampledTileData"));
	AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(DownsampledTileAllocator), 0);

	// #sdl_todo: merge classification passes or reuse downsampled one to create full res tiles
	// Run tile classification to generate tiles for the subsequent passes
	{
		{
			FTileClassificationCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FTileClassificationCS::FParameters>();
			PassParameters->StochasticDirectLightingParameters = StochasticDirectLightingParameters;
			PassParameters->RWTileAllocator = GraphBuilder.CreateUAV(TileAllocator);
			PassParameters->RWTileData = GraphBuilder.CreateUAV(TileData);

			FTileClassificationCS::FPermutationDomain PermutationVector;
			PermutationVector.Set<FTileClassificationCS::FDownsampledClassification>(false);
			auto ComputeShader = View.ShaderMap->GetShader<FTileClassificationCS>(PermutationVector);

			const FIntVector GroupCount = FComputeShaderUtils::GetGroupCount(View.ViewRect.Size(), FTileClassificationCS::GetGroupSize());

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("TileClassification %dx%d", View.ViewRect.Size().X, View.ViewRect.Size().Y),
				ComputeShader,
				PassParameters,
				GroupCount);
		}

		{
			FTileClassificationCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FTileClassificationCS::FParameters>();
			PassParameters->StochasticDirectLightingParameters = StochasticDirectLightingParameters;
			PassParameters->RWTileAllocator = GraphBuilder.CreateUAV(DownsampledTileAllocator);
			PassParameters->RWTileData = GraphBuilder.CreateUAV(DownsampledTileData);

			FTileClassificationCS::FPermutationDomain PermutationVector;
			PermutationVector.Set<FTileClassificationCS::FDownsampledClassification>(true);
			auto ComputeShader = View.ShaderMap->GetShader<FTileClassificationCS>(PermutationVector);

			const FIntVector GroupCount = FComputeShaderUtils::GetGroupCount(View.ViewRect.Size(), FTileClassificationCS::GetGroupSize());

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("DownsampledTileClassification %dx%d", DownsampledViewSize.X, DownsampledViewSize.Y),
				ComputeShader,
				PassParameters,
				GroupCount);
		}
	}

	FRDGBufferRef TileIndirectArgs = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateIndirectDesc<FRHIDispatchIndirectParameters>((int32)StochasticDirectLighting::ETileType::MAX), TEXT("StochasticDirectLighting.TileIndirectArgs"));
	FRDGBufferRef DownsampledTileIndirectArgs = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateIndirectDesc<FRHIDispatchIndirectParameters>((int32)StochasticDirectLighting::ETileType::MAX), TEXT("StochasticDirectLighting.DownsampledTileIndirectArgs"));

	// Setup indirect args for classified tiles
	{
		FInitTileIndirectArgsCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FInitTileIndirectArgsCS::FParameters>();
		PassParameters->StochasticDirectLightingParameters = StochasticDirectLightingParameters;
		PassParameters->RWTileIndirectArgs = GraphBuilder.CreateUAV(TileIndirectArgs);
		PassParameters->RWDownsampledTileIndirectArgs = GraphBuilder.CreateUAV(DownsampledTileIndirectArgs);
		PassParameters->TileAllocator = GraphBuilder.CreateSRV(TileAllocator);
		PassParameters->DownsampledTileAllocator = GraphBuilder.CreateSRV(DownsampledTileAllocator);

		auto ComputeShader = View.ShaderMap->GetShader<FInitTileIndirectArgsCS>();

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("InitTileIndirectArgs"),
			ComputeShader,
			PassParameters,
			FIntVector(1, 1, 1));
	}

	FRDGBufferRef CompositeTileAllocator = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), 1), TEXT("StochasticDirectLighting.CompositeTileAllocator"));
	FRDGBufferRef CompositeTileData = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(2 * sizeof(uint32), MaxCompositeTiles), TEXT("StochasticDirectLighting.CompositeTileData"));
	AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(CompositeTileAllocator), 0);
	
	// Generate new candidate light samples
	{
		FRDGTextureUAVRef DownsampledSceneDepthUAV = GraphBuilder.CreateUAV(DownsampledSceneDepth, ERDGUnorderedAccessViewFlags::SkipBarrier);
		FRDGTextureUAVRef DownsampledSceneWorldNormalUAV = GraphBuilder.CreateUAV(DownsampledSceneWorldNormal, ERDGUnorderedAccessViewFlags::SkipBarrier);
		FRDGBufferUAVRef CompositeTileAllocatorUAV = GraphBuilder.CreateUAV(CompositeTileAllocator, ERDGUnorderedAccessViewFlags::SkipBarrier);
		FRDGBufferUAVRef CompositeTileDataUAV = GraphBuilder.CreateUAV(CompositeTileData, ERDGUnorderedAccessViewFlags::SkipBarrier);
		FRDGTextureUAVRef LightSamplesUAV = GraphBuilder.CreateUAV(LightSamples, ERDGUnorderedAccessViewFlags::SkipBarrier);

		// Clear tiles which don't contain any lights or geometry
		{
			FClearLightSamplesCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FClearLightSamplesCS::FParameters>();
			PassParameters->IndirectArgs = DownsampledTileIndirectArgs;
			PassParameters->StochasticDirectLightingParameters = StochasticDirectLightingParameters;
			PassParameters->RWDownsampledSceneDepth = DownsampledSceneDepthUAV;
			PassParameters->RWDownsampledSceneWorldNormal = DownsampledSceneWorldNormalUAV;
			PassParameters->RWLightSamples = LightSamplesUAV;
			PassParameters->DownsampledTileAllocator = GraphBuilder.CreateSRV(DownsampledTileAllocator);
			PassParameters->DownsampledTileData = GraphBuilder.CreateSRV(DownsampledTileData);

			FClearLightSamplesCS::FPermutationDomain PermutationVector;
			PermutationVector.Set<FClearLightSamplesCS::FDebugMode>(bDebug);
			auto ComputeShader = View.ShaderMap->GetShader<FClearLightSamplesCS>(PermutationVector);

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("ClearLightSamples"),
				ComputeShader,
				PassParameters,
				DownsampledTileIndirectArgs,
				(int32)StochasticDirectLighting::ETileType::Empty * sizeof(FRHIDispatchIndirectParameters));
		}

		for (int32 TileType = 0; TileType < (int32)StochasticDirectLighting::ETileType::SHADING_MAX; ++TileType)
		{
			FGenerateSamplesCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FGenerateSamplesCS::FParameters>();
			PassParameters->IndirectArgs = DownsampledTileIndirectArgs;
			PassParameters->StochasticDirectLightingParameters = StochasticDirectLightingParameters;
			PassParameters->RWDownsampledSceneDepth = DownsampledSceneDepthUAV;
			PassParameters->RWDownsampledSceneWorldNormal = DownsampledSceneWorldNormalUAV;
			PassParameters->RWLightSamples = LightSamplesUAV;
			PassParameters->RWCompositeTileAllocator = CompositeTileAllocatorUAV;
			PassParameters->RWCompositeTileData = CompositeTileDataUAV;
			PassParameters->DownsampledTileAllocator = GraphBuilder.CreateSRV(DownsampledTileAllocator);
			PassParameters->DownsampledTileData = GraphBuilder.CreateSRV(DownsampledTileData);
			PassParameters->HistoryScreenPositionScaleBias = HistoryScreenPositionScaleBias;
			PassParameters->HistoryUVMinMax = HistoryUVMinMax;

			FGenerateSamplesCS::FPermutationDomain PermutationVector;
			PermutationVector.Set<FGenerateSamplesCS::FTileType>(TileType);
			PermutationVector.Set<FGenerateSamplesCS::FIESProfile>(CVarStochasticDirectLightingIESProfiles.GetValueOnRenderThread() != 0);
			PermutationVector.Set<FGenerateSamplesCS::FLightFunctionAtlas>(bUseLightFunctionAtlas);
			PermutationVector.Set<FGenerateSamplesCS::FTexturedRectLights>(CVarStochasticDirectLightingTexturedRectLights.GetValueOnRenderThread() != 0);
			PermutationVector.Set<FGenerateSamplesCS::FNumSamplesPerPixel1d>(NumSamplesPerPixel2d.X * NumSamplesPerPixel2d.Y);
			PermutationVector.Set<FGenerateSamplesCS::FDebugMode>(bDebug);
			PermutationVector.Set<FGenerateSamplesCS::FCandidateLightMask>(CVarStochasticDirectLightingCandidateLightMask.GetValueOnRenderThread() != 0);
			auto ComputeShader = View.ShaderMap->GetShader<FGenerateSamplesCS>(PermutationVector);

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("GenerateSamples SamplesPerPixel:%dx%d TileType:%d", NumSamplesPerPixel2d.X, NumSamplesPerPixel2d.Y, TileType),
				ComputeShader,
				PassParameters,
				DownsampledTileIndirectArgs,
				TileType * sizeof(FRHIDispatchIndirectParameters));
		}
	}

	FRDGBufferRef CompositeTileIndirectArgs = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateIndirectDesc<FRHIDispatchIndirectParameters>(1), TEXT("StochasticDirectLighting.CompositeTileIndirectArgs"));

	// Setup indirect args for shadow mask tile updates
	{
		FInitCompositeIndirectArgsCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FInitCompositeIndirectArgsCS::FParameters>();
		PassParameters->StochasticDirectLightingParameters = StochasticDirectLightingParameters;
		PassParameters->RWIndirectArgs = GraphBuilder.CreateUAV(CompositeTileIndirectArgs);
		PassParameters->CompositeTileAllocator = GraphBuilder.CreateSRV(CompositeTileAllocator);

		auto ComputeShader = View.ShaderMap->GetShader<FInitCompositeIndirectArgsCS>();

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("InitCompositeIndirectArgs"),
			ComputeShader,
			PassParameters,
			FIntVector(1, 1, 1));
	}

	StochasticDirectLighting::RayTraceLightSamples(
		View,
		GraphBuilder, 
		SceneTextures,
		SampleBufferSize,
		LightSamples,
		LightSampleRayDistance,
		StochasticDirectLightingParameters
	);

	FRDGTextureRef CompositeUpsampleWeights = GraphBuilder.CreateTexture(
		FRDGTextureDesc::Create2D(SceneTextures.Config.Extent, PF_FloatRGBA, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV),
		TEXT("StochasticDirectLighting.CompositeUpsampleWeights"));

	// Init composite upsample weights
	{
		FInitCompositeUpsampleWeightsCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FInitCompositeUpsampleWeightsCS::FParameters>();
		PassParameters->StochasticDirectLightingParameters = StochasticDirectLightingParameters;
		PassParameters->RWCompositeUpsampleWeights = GraphBuilder.CreateUAV(CompositeUpsampleWeights);

		auto ComputeShader = View.ShaderMap->GetShader<FInitCompositeUpsampleWeightsCS>();

		const FIntVector GroupCount = FComputeShaderUtils::GetGroupCount(View.ViewRect.Size(), FInitCompositeUpsampleWeightsCS::GetGroupSize());

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("InitCompositeUpsampleWeights"),
			ComputeShader,
			PassParameters,
			GroupCount);
	}

	FRDGBufferRef ShadingTileAllocator = GraphBuilder.CreateBuffer(
		FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), 1),
		TEXT("StochasticDirectLightingParameters.ShadingTileAllocator"));

	FRDGTextureRef ShadingTileGridAllocator = GraphBuilder.CreateTexture(
		FRDGTextureDesc::Create2D(ShadingTileGridSize, PF_R32_UINT, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV),
		TEXT("StochasticDirectLighting.ShadingTileGridAllocator"));

	FRDGBufferRef ShadingTileGrid = GraphBuilder.CreateBuffer(
		FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), ShadingTileGridSize.X * ShadingTileGridSize.Y * MaxShadingTilesPerGridCell),
		TEXT("StochasticDirectLightingParameters.ShadingTileGrid"));

	FRDGTextureRef ShadingTileAtlas = GraphBuilder.CreateTexture(
		FRDGTextureDesc::Create2D(ShadingTileAtlasSize, PF_R16F, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV),
		TEXT("StochasticDirectLighting	.ShadingTileAtlas"));

	AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(ShadingTileAllocator), 0u);
	AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(ShadingTileGridAllocator), 0u);

	// Composite shadow masks traces
	{
		FCompositeLightSamplesCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FCompositeLightSamplesCS::FParameters>();
		PassParameters->StochasticDirectLightingParameters = StochasticDirectLightingParameters;
		PassParameters->IndirectArgs = CompositeTileIndirectArgs;
		PassParameters->RWShadingTileAllocator = GraphBuilder.CreateUAV(ShadingTileAllocator);
		PassParameters->RWShadingTileGridAllocator = GraphBuilder.CreateUAV(ShadingTileGridAllocator);
		PassParameters->RWShadingTileGrid = GraphBuilder.CreateUAV(ShadingTileGrid);
		PassParameters->RWShadingTileAtlas = GraphBuilder.CreateUAV(ShadingTileAtlas);
		PassParameters->CompositeTileData = GraphBuilder.CreateSRV(CompositeTileData);
		PassParameters->CompositeUpsampleWeights = CompositeUpsampleWeights;
		PassParameters->LightSamples = LightSamples;

		FCompositeLightSamplesCS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FCompositeLightSamplesCS::FDebugMode>(bDebug);
		auto ComputeShader = View.ShaderMap->GetShader<FCompositeLightSamplesCS>(PermutationVector);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("CompositeLightSamples"),
			ComputeShader,
			PassParameters,
			CompositeTileIndirectArgs,
			0);
	}

	FRDGTextureRef ResolvedDiffuseLighting = GraphBuilder.CreateTexture(
		FRDGTextureDesc::Create2D(View.GetSceneTexturesConfig().Extent, PF_FloatRGB, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV),
		TEXT("StochasticDirectLighting.ResolvedDiffuseLighting"));

	FRDGTextureRef ResolvedSpecularLighting = GraphBuilder.CreateTexture(
		FRDGTextureDesc::Create2D(View.GetSceneTexturesConfig().Extent, PF_FloatRGB, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV),
		TEXT("StochasticDirectLighting.ResolvedSpecularLighting"));

	// Shade light samples
	{
		FRDGTextureUAVRef ResolvedDiffuseLightingUAV = GraphBuilder.CreateUAV(ResolvedDiffuseLighting, ERDGUnorderedAccessViewFlags::SkipBarrier);
		FRDGTextureUAVRef ResolvedSpecularLightingUAV = GraphBuilder.CreateUAV(ResolvedSpecularLighting, ERDGUnorderedAccessViewFlags::SkipBarrier);

		for (int32 TileType = 0; TileType < (int32)StochasticDirectLighting::ETileType::SHADING_MAX; ++TileType)
		{
			FShadeLightSamplesCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FShadeLightSamplesCS::FParameters>();
			PassParameters->RWResolvedDiffuseLighting = ResolvedDiffuseLightingUAV;
			PassParameters->RWResolvedSpecularLighting = ResolvedSpecularLightingUAV;
			PassParameters->IndirectArgs = TileIndirectArgs;
			PassParameters->StochasticDirectLightingParameters = StochasticDirectLightingParameters;
			PassParameters->CompositeTileAllocator = GraphBuilder.CreateSRV(CompositeTileAllocator);
			PassParameters->TileAllocator = GraphBuilder.CreateSRV(TileAllocator);
			PassParameters->TileData = GraphBuilder.CreateSRV(TileData);
			PassParameters->ShadingTileAllocator = GraphBuilder.CreateSRV(ShadingTileAllocator);
			PassParameters->ShadingTileGridAllocator = ShadingTileGridAllocator;
			PassParameters->ShadingTileGrid = GraphBuilder.CreateSRV(ShadingTileGrid);
			PassParameters->ShadingTileAtlas = ShadingTileAtlas;

			FShadeLightSamplesCS::FPermutationDomain PermutationVector;
			PermutationVector.Set<FShadeLightSamplesCS::FTileType>(TileType);
			PermutationVector.Set<FShadeLightSamplesCS::FIESProfile>(CVarStochasticDirectLightingIESProfiles.GetValueOnRenderThread() != 0);
			PermutationVector.Set<FShadeLightSamplesCS::FLightFunctionAtlas>(bUseLightFunctionAtlas);
			PermutationVector.Set<FShadeLightSamplesCS::FTexturedRectLights>(CVarStochasticDirectLightingTexturedRectLights.GetValueOnRenderThread() != 0);
			PermutationVector.Set<FShadeLightSamplesCS::FDebugMode>(bDebug);
			auto ComputeShader = View.ShaderMap->GetShader<FShadeLightSamplesCS>(PermutationVector);

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("ShadeLightSamples TileType:%d", TileType),
				ComputeShader,
				PassParameters,
				TileIndirectArgs,
				TileType * sizeof(FRHIDispatchIndirectParameters));
		}
	}

	// Demodulated lighting components with second luminance moments stored in alpha channel for temporal variance tracking
	// This will be passed to the next frame
	FRDGTextureRef DiffuseLightingAndSecondMoment = GraphBuilder.CreateTexture(
		FRDGTextureDesc::Create2D(View.GetSceneTexturesConfig().Extent, PF_FloatRGBA, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV),
		TEXT("StochasticDirectLighting.DiffuseLightingAndSecondMoment"));

	FRDGTextureRef SpecularLightingAndSecondMoment = GraphBuilder.CreateTexture(
		FRDGTextureDesc::Create2D(View.GetSceneTexturesConfig().Extent, PF_FloatRGBA, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV),
		TEXT("StochasticDirectLighting.SpecularLightingAndSecondMoment"));

	FRDGTextureRef SceneDepthCopy = GraphBuilder.CreateTexture(
		FRDGTextureDesc::Create2D(SceneTextures.Depth.Resolve->Desc.Extent, PF_R32_FLOAT, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV),
		TEXT("StochasticDirectLighting.DepthHistory"));

	FRDGTextureRef NumFramesAccumulated = GraphBuilder.CreateTexture(
		FRDGTextureDesc::Create2D(View.GetSceneTexturesConfig().Extent, PF_G8, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV),
		TEXT("StochasticDirectLighting.NumFramesAccumulated"));

	// Temporal accumulation
	{
		FSDLTemporalAccumulationCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FSDLTemporalAccumulationCS::FParameters>();
		PassParameters->StochasticDirectLightingParameters = StochasticDirectLightingParameters;
		PassParameters->ResolvedDiffuseLighting = ResolvedDiffuseLighting;
		PassParameters->ResolvedSpecularLighting = ResolvedSpecularLighting;
		PassParameters->DiffuseLightingAndSecondMomentHistoryTexture = DiffuseLightingAndSecondMomentHistory;
		PassParameters->SpecularLightingAndSecondMomentHistoryTexture = SpecularLightingAndSecondMomentHistory;
		PassParameters->NumFramesAccumulatedHistoryTexture = NumFramesAccumulatedHistory;
		PassParameters->StochasticDirectLightingDepthHistory = SceneDepthHistory;
		PassParameters->PrevSceneColorPreExposureCorrection = View.PreExposure / View.PrevViewInfo.SceneColorPreExposure;
		PassParameters->HistoryScreenPositionScaleBias = HistoryScreenPositionScaleBias;
		PassParameters->HistoryUVMinMax = HistoryUVMinMax;
		PassParameters->RWDiffuseLightingAndSecondMoment = GraphBuilder.CreateUAV(DiffuseLightingAndSecondMoment);
		PassParameters->RWSpecularLightingAndSecondMoment = GraphBuilder.CreateUAV(SpecularLightingAndSecondMoment);
		PassParameters->RWNumFramesAccumulated = GraphBuilder.CreateUAV(NumFramesAccumulated);
		PassParameters->RWSceneDepth = GraphBuilder.CreateUAV(SceneDepthCopy);
		PassParameters->RWSceneColor = GraphBuilder.CreateUAV(SceneTextures.Color.Target);

		FSDLTemporalAccumulationCS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FSDLTemporalAccumulationCS::FValidHistory>(DiffuseLightingAndSecondMomentHistory != nullptr && bTemporal);
		PermutationVector.Set<FSDLTemporalAccumulationCS::FDebugMode>(bDebug);
		auto ComputeShader = View.ShaderMap->GetShader<FSDLTemporalAccumulationCS>(PermutationVector);

		const FIntVector GroupCount = FComputeShaderUtils::GetGroupCount(View.ViewRect.Size(), FSDLTemporalAccumulationCS::GetGroupSize());

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("TemporalAccumulation"),
			ComputeShader,
			PassParameters,
			GroupCount);
	}

	// Spatial filter
	{
		FSDLSpatialFilterCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FSDLSpatialFilterCS::FParameters>();
		PassParameters->StochasticDirectLightingParameters = StochasticDirectLightingParameters;
		PassParameters->RWSceneColor = GraphBuilder.CreateUAV(SceneTextures.Color.Target);
		PassParameters->DiffuseLightingAndSecondMomentTexture = DiffuseLightingAndSecondMoment;
		PassParameters->SpecularLightingAndSecondMomentTexture = SpecularLightingAndSecondMoment;
		PassParameters->SpatialFilterDepthWeightScale = CVarStochasticDirectLightingSpatialDepthWeightScale.GetValueOnRenderThread();

		FSDLSpatialFilterCS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FSDLSpatialFilterCS::FSpatialFilter>(CVarStochasticDirectLightingSpatial.GetValueOnRenderThread() != 0);
		PermutationVector.Set<FSDLSpatialFilterCS::FDebugMode>(bDebug);
		auto ComputeShader = View.ShaderMap->GetShader<FSDLSpatialFilterCS>(PermutationVector);

		const FIntVector GroupCount = FComputeShaderUtils::GetGroupCount(View.ViewRect.Size(), FSDLSpatialFilterCS::GetGroupSize());

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("Spatial"),
			ComputeShader,
			PassParameters,
			GroupCount);
	}

	if (View.ViewState && !View.bStatePrevViewInfoIsReadOnly)
	{
		FStochasticDirectLightingViewState& LightingViewState = View.ViewState->StochasticDirectLighting;

		LightingViewState.HistoryScreenPositionScaleBias = View.GetScreenPositionScaleBias(View.GetSceneTexturesConfig().Extent, View.ViewRect);

		// Pull in the max UV to exclude the region which will read outside the viewport due to bilinear filtering
		const FVector2D InvBufferSize(1.0f / SceneTextures.Config.Extent.X, 1.0f / SceneTextures.Config.Extent.Y);
		LightingViewState.HistoryUVMinMax = FVector4f(
			(View.ViewRect.Min.X + 0.5f) * InvBufferSize.X,
			(View.ViewRect.Min.Y + 0.5f) * InvBufferSize.Y,
			(View.ViewRect.Max.X - 1.0f) * InvBufferSize.X,
			(View.ViewRect.Max.Y - 1.0f) * InvBufferSize.Y);

		if (DiffuseLightingAndSecondMoment && SpecularLightingAndSecondMoment && SceneDepthCopy && NumFramesAccumulated && bTemporal)
		{
			GraphBuilder.QueueTextureExtraction(DiffuseLightingAndSecondMoment, &LightingViewState.DiffuseLightingAndSecondMomentHistory);
			GraphBuilder.QueueTextureExtraction(SpecularLightingAndSecondMoment, &LightingViewState.SpecularLightingAndSecondMomentHistory);
			GraphBuilder.QueueTextureExtraction(SceneDepthCopy, &LightingViewState.SceneDepthHistory);
			GraphBuilder.QueueTextureExtraction(NumFramesAccumulated, &LightingViewState.NumFramesAccumulatedHistory);
		}
		else
		{
			LightingViewState.DiffuseLightingAndSecondMomentHistory = nullptr;
			LightingViewState.SpecularLightingAndSecondMomentHistory = nullptr;
			LightingViewState.SceneDepthHistory = nullptr;
			LightingViewState.NumFramesAccumulatedHistory = nullptr;
		}
	}
}