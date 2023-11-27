// Copyright Epic Games, Inc. All Rights Reserved.

#include "StochasticShadows.h"
#include "StochasticShadowsInternal.h"
#include "RendererPrivate.h"
#include "PixelShaderUtils.h"
#include "BasePassRendering.h"

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

static TAutoConsoleVariable<int32> CVarStochasticShadowsTemporal(
	TEXT("r.StochasticShadows.Temporal"),
	1,
	TEXT("Whether to use temporal accumulation for shadow mask."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarStochasticShadowsTemporalMaxFramesAccumulated(
	TEXT("r.StochasticShadows.Temporal.MaxFramesAccumulated"),
	8,
	TEXT("Max history length when accumulating frames. Lower values have less ghosting, but more noise."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> CVarStochasticShadowsTemporalStdDevOffset(
	TEXT("r.StochasticShadows.Temporal.StdDevOffset"),
	0.1f,
	TEXT("Increases standard deviation in neighborhood clamp. Higher values cause more ghosting, but allow smoother temporal accumulation."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> CVarStochasticShadowsSamplingMinWeight(
	TEXT("r.StochasticShadows.Sampling.MinWeight"),
	0.002f,
	TEXT("Determines minimal sample influence on final pixels. Used to skip samples which would have minimal impact to the final image even if light is fully visible."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarStochasticSamplingShadowEstimate(
	TEXT("r.StochasticShadows.Sampling.ShadowEstimate"),
	0,
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

static TAutoConsoleVariable<int32> CVarStochasticShadowsDebugLightId(
	TEXT("r.StochasticShadows.Debug.LightId"),
	-1,
	TEXT("Which light to show debug info for. When set to -1, uses the currently selected light in editor."),
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

static TAutoConsoleVariable<int> CVarStochasticShadowsCandidateLightMask(
	TEXT("r.StochasticShadows.CandidateLightMask"),
	1,
	TEXT("#sdl_todo: finish and pick one shader path."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int> CVarStochasticShadowsTexturedRectLights(
	TEXT("r.StochasticShadows.TexturedRectLights"),
	0,
	TEXT("Whether to support textured rect lights."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

namespace StochasticShadows
{
	// must match values in StochasticShadows.ush
	constexpr int32 TileSize = 8;
	constexpr int32 ShadowMaskTileSize = 8;	// Stored downsampled
	constexpr int32 MaxLightSceneIdXY = 16; // 16 * 16 = 256
	constexpr int32 MaxShadingTilesPerGridCell = 32;
	constexpr int32 ShadowMaskAtlasSizeInTiles = 512;
	constexpr uint32 InvalidShadowMaskTileIndex = 0xFFFFFFFF;

	bool IsEnabled()
	{
		return CVarStochasticShadows.GetValueOnRenderThread() != 0;
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

	uint32 GetStateFrameIndex(FSceneViewState* ViewState)
	{
		uint32 StateFrameIndex = ViewState ? ViewState->GetFrameIndex() : 0;

		if (CVarStochasticShadowsFixedStateFrameIndex.GetValueOnRenderThread() >= 0)
		{
			StateFrameIndex = CVarStochasticShadowsFixedStateFrameIndex.GetValueOnRenderThread();
		}
		
		return StateFrameIndex;
	}

	FIntPoint GetNumSamplesPerPixel2d()
	{
		const uint32 NumSamplesPerPixel1d = FMath::RoundUpToPowerOfTwo(FMath::Clamp(CVarStochasticShadowsNumSamplesPerPixel.GetValueOnRenderThread(), 1, 4));
		return NumSamplesPerPixel1d == 4 ? FIntPoint(2, 2) : (NumSamplesPerPixel1d == 2 ? FIntPoint(2, 1) : FIntPoint(1, 1));
	}

	int32 GetDebugMode()
	{
		return CVarStochasticShadowsDebug.GetValueOnRenderThread();
	}

	bool UseWaveOps(EShaderPlatform ShaderPlatform)
	{
		return CVarStochasticShadowsWaveOps.GetValueOnRenderThread() != 0 
			&& GRHISupportsWaveOperations
			&& RHISupportsWaveOperations(ShaderPlatform);
	}

	// Keep in sync with TILE_TYPE_* in shaders
	enum class ETileType : uint8
	{
		SimpleShading = 0,
		ComplexShading = 1,
		MAX = 2
	};
}

class FTileClassificationCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FTileClassificationCS)
	SHADER_USE_PARAMETER_STRUCT(FTileClassificationCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FStochasticShadowsParameters, StochasticShadowsParameters)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, RWTileAllocator)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, RWTileData)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<uint>, RWDownsampledTileMask)
	END_SHADER_PARAMETER_STRUCT()

	class FDownsampledClassification : SHADER_PERMUTATION_BOOL("DOWNSAMPLED_CLASSIFICATION");
	using FPermutationDomain = TShaderPermutationDomain<FDownsampledClassification>;

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

IMPLEMENT_GLOBAL_SHADER(FTileClassificationCS, "/Engine/Private/StochasticShadows/StochasticShadows.usf", "TileClassificationCS", SF_Compute);

class FInitTileIndirectArgsCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FInitTileIndirectArgsCS)
	SHADER_USE_PARAMETER_STRUCT(FInitTileIndirectArgsCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FStochasticShadowsParameters, StochasticShadowsParameters)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWTileIndirectArgs)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWDownsampledTileIndirectArgs)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, TileAllocator)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, DownsampledTileAllocator)
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

IMPLEMENT_GLOBAL_SHADER(FInitTileIndirectArgsCS, "/Engine/Private/StochasticShadows/StochasticShadows.usf", "InitTileIndirectArgsCS", SF_Compute);

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
		RDG_BUFFER_ACCESS(IndirectArgs, ERHIAccess::IndirectArgs)
		SHADER_PARAMETER_STRUCT_INCLUDE(FStochasticShadowsParameters, StochasticShadowsParameters)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float>, RWDownsampledSceneDepth)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<UNORM float3>, RWDownsampledSceneWorldNormal)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<uint32>, RWShadowMaskHistoryScreenCoord00)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, RWShadowMaskHistoryWeights)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, RWShadowMaskTileAllocator)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint2>, RWShadowMaskTileData)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<uint>, RWShadowMaskPageTable)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<uint>, RWLightSamples)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FForwardLightData, ForwardLightData)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, DownsampledTileAllocator)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, DownsampledTileData)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<uint>, ShadowMaskPageTableHistory)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<uint>, ShadowMaskAtlasHistory)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<uint>, ShadowMaskTileAtlas)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float>, ShadowMaskSceneDepthHistory)
		SHADER_PARAMETER(FVector4f, HistoryUVMinMax)
		SHADER_PARAMETER(FVector4f, HistoryScreenPositionScaleBias)
	END_SHADER_PARAMETER_STRUCT()

	class FTileType : SHADER_PERMUTATION_INT("TILE_TYPE", (int32)StochasticShadows::ETileType::MAX);
	class FTexturedRectLights : SHADER_PERMUTATION_BOOL("USE_SOURCE_TEXTURE");
	class FNumSamplesPerPixel1d : SHADER_PERMUTATION_SPARSE_INT("NUM_SAMPLES_PER_PIXEL_1D", 1, 2, 4);
	class FShadowMaskReprojectionWeights : SHADER_PERMUTATION_BOOL("SHADOW_MASK_REPROJECTION_WEIGHTS");
	class FShadowFactorEstimate : SHADER_PERMUTATION_BOOL("SHADOW_FACTOR_ESTIMATE");
	class FCandidateLightMask : SHADER_PERMUTATION_BOOL("CANDIDATE_LIGHT_MASK");
	class FDebugMode : SHADER_PERMUTATION_BOOL("DEBUG_MODE");
	using FPermutationDomain = TShaderPermutationDomain<FTileType, FTexturedRectLights, FNumSamplesPerPixel1d, FShadowMaskReprojectionWeights, FShadowFactorEstimate, FCandidateLightMask, FDebugMode>;

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
	}
};

IMPLEMENT_GLOBAL_SHADER(FGenerateSamplesCS, "/Engine/Private/StochasticShadows/StochasticShadowsSampling.usf", "GenerateSamplesCS", SF_Compute);

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
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint2>, ShadowMaskTileData)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<uint32>, ShadowMaskHistoryScreenCoord00)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float4>, ShadowMaskHistoryWeights)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<uint>, ShadowMaskPageTableHistory)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<uint>, ShadowMaskAtlasHistory)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<uint>, LightSamples)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FForwardLightData, ForwardLightData)
	END_SHADER_PARAMETER_STRUCT()

	static int32 GetGroupSize()
	{
		return 8;
	}

	class FTemporalAccumulation : SHADER_PERMUTATION_BOOL("TEMPORAL_ACCUMULATION");
	class FDebugMode : SHADER_PERMUTATION_BOOL("DEBUG_MODE");
	using FPermutationDomain = TShaderPermutationDomain<FTemporalAccumulation, FDebugMode>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return StochasticShadows::ShouldCompileShaders(Parameters);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		FForwardLightingParameters::ModifyCompilationEnvironment(Parameters.Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());
	}
};

IMPLEMENT_GLOBAL_SHADER(FCompositeShadowMaskTracesCS, "/Engine/Private/StochasticShadows/StochasticShadowsComposite.usf", "CompositeShadowMaskTracesCS", SF_Compute);

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
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint2>, ShadowMaskTileData)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<uint>, ShadowMaskPageTable)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<uint>, ShadowMaskTileAtlas)
	END_SHADER_PARAMETER_STRUCT()

	static int32 GetGroupSize()
	{
		return 8;
	}

	class FSpatialPass : SHADER_PERMUTATION_BOOL("SPATIAL_PASS");
	using FPermutationDomain = TShaderPermutationDomain<FSpatialPass>;

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

class FShadeLightSamplesCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FShadeLightSamplesCS)
	SHADER_USE_PARAMETER_STRUCT(FShadeLightSamplesCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		RDG_BUFFER_ACCESS(IndirectArgs, ERHIAccess::IndirectArgs)
		SHADER_PARAMETER_STRUCT_INCLUDE(FStochasticShadowsParameters, StochasticShadowsParameters)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, RWSceneColor)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<uint>, ShadowMaskPageTable)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<uint>, ShadowMaskTileAtlas)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, TileAllocator)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, TileData)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, ShadowMaskTileAllocator)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, ShadingTileAllocator)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<uint>, ShadingTileGridAllocator)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, ShadingTileGrid)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, ShadingTiles)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<UNORM float>, ShadingTileAtlas)
	END_SHADER_PARAMETER_STRUCT()

	static int32 GetGroupSize()
	{
		return 8;
	}

	class FTileType : SHADER_PERMUTATION_INT("TILE_TYPE", (int32)StochasticShadows::ETileType::MAX);
	class FTexturedRectLights : SHADER_PERMUTATION_BOOL("USE_SOURCE_TEXTURE");
	class FDebugMode : SHADER_PERMUTATION_BOOL("DEBUG_MODE");
	using FPermutationDomain = TShaderPermutationDomain<FTileType, FTexturedRectLights, FDebugMode>;

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

IMPLEMENT_GLOBAL_SHADER(FShadeLightSamplesCS, "/Engine/Private/StochasticShadows/StochasticShadowsShading.usf", "ShadeLightSamplesCS", SF_Compute);

/**
 * Single pass batched light rendering using ray tracing (distance field or triangle) for shadowing.
 */
void FDeferredShadingSceneRenderer::RenderStochasticShadows(FRDGBuilder& GraphBuilder, const FSceneTextures& SceneTextures)
{
	if (!StochasticShadows::IsEnabled())
	{
		return;
	}

	check(AreLightsInLightGrid());

	RDG_EVENT_SCOPE(GraphBuilder, "StochasticShadows");

	const FViewInfo& View = Views[0];
	FBlueNoise BlueNoise = GetBlueNoiseGlobalParameters();
	TUniformBufferRef<FBlueNoise> BlueNoiseUniformBuffer = CreateUniformBufferImmediate(BlueNoise, EUniformBufferUsage::UniformBuffer_SingleDraw);

	const bool bDebug = StochasticShadows::GetDebugMode() != 0;
	const bool bWaveOps = StochasticShadows::UseWaveOps(View.GetShaderPlatform())
		&& GRHIMinimumWaveSize <= 32
		&& GRHIMaximumWaveSize >= 32;

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

	const FIntPoint NumSamplesPerPixel2d = StochasticShadows::GetNumSamplesPerPixel2d();

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

	const FIntPoint ShadingTileGridSize = FIntPoint::DivideAndRoundUp(SceneTextures.Config.Extent, StochasticShadows::ShadowMaskTileSize);
	const FIntPoint ShadingTileAtlasSize = ShadowMaskTileAtlasSize;

	FRDGTextureRef DownsampledSceneDepth = GraphBuilder.CreateTexture(
		FRDGTextureDesc::Create2D(DownsampledBufferSize, PF_R32_FLOAT, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV),
		TEXT("StochasticShadows.DownsampledSceneDepth"));

	FRDGTextureRef DownsampledSceneWorldNormal = GraphBuilder.CreateTexture(
		FRDGTextureDesc::Create2D(DownsampledBufferSize, PF_A2B10G10R10, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV),
		TEXT("StochasticShadows.DownsampledSceneWorldNormal"));

	FRDGTextureRef LightSamples = GraphBuilder.CreateTexture(
		FRDGTextureDesc::Create2D(DonwnsampledSampleBufferSize, PF_R16_UINT, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV),
		TEXT("StochasticShadows.LightSamples"));

	FRDGTextureRef LightSampleRayDistance = GraphBuilder.CreateTexture(
		FRDGTextureDesc::Create2D(DonwnsampledSampleBufferSize, PF_R16F, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV),
		TEXT("StochasticShadows.LightSampleRayDistance"));
	
	FRDGTextureRef ShadowMaskPageTable = GraphBuilder.CreateTexture(
		FRDGTextureDesc::Create2D(ShadowMaskPageTableSize, PF_R32_UINT, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV),
		TEXT("StochasticShadows.ShadowMaskPageTable"));

	AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(ShadowMaskPageTable), StochasticShadows::InvalidShadowMaskTileIndex);

	const EPixelFormat ShadowMaskTileAtlasFormat = PF_R32_UINT;
	FRDGTextureRef ShadowMaskTileAtlas = GraphBuilder.CreateTexture(
		FRDGTextureDesc::Create2D(ShadowMaskTileAtlasSize, ShadowMaskTileAtlasFormat, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV),
		TEXT("StochasticShadows.ShadowMaskTileAtlas"));
	
	bool bTemporal = CVarStochasticShadowsTemporal.GetValueOnRenderThread() != 0;
	FRDGTextureRef ShadowMaskSceneDepthHistory = nullptr;
	FRDGTextureRef ShadowMaskPageTableHistory = nullptr;
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

			if (LightingViewState.ShadowMaskAtlasHistory 
				&& LightingViewState.ShadowMaskAtlasHistory->GetDesc().Extent == ShadowMaskTileAtlasSize)
			{
				ShadowMaskAtlasHistory = GraphBuilder.RegisterExternalTexture(LightingViewState.ShadowMaskAtlasHistory);
				ShadowMaskSceneDepthHistory = LightingViewState.ShadowMaskSceneDepthHistory ? GraphBuilder.RegisterExternalTexture(LightingViewState.ShadowMaskSceneDepthHistory) : nullptr;
			}
		}
	}

	if (ShadowMaskAtlasHistory == nullptr || ShadowMaskPageTableHistory == nullptr)
	{
		bTemporal = false;
	}

	const FIntPoint ViewSizeInTiles = FIntPoint::DivideAndRoundUp(View.ViewRect.Size(), StochasticShadows::TileSize);
	const int32 TileDataStride = ViewSizeInTiles.X * ViewSizeInTiles.Y;

	const FIntPoint DownsampledViewSizeInTiles = FIntPoint::DivideAndRoundUp(DownsampledViewSize, StochasticShadows::TileSize);
	const int32 DownsampledTileDataStride = DownsampledViewSizeInTiles.X * DownsampledViewSizeInTiles.Y;

	FRDGTextureRef DownsampledTileMask = GraphBuilder.CreateTexture(
		FRDGTextureDesc::Create2D(FMath::DivideAndRoundUp<FIntPoint>(DownsampledBufferSize, StochasticShadows::TileSize), PF_R8_UINT, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV),
		TEXT("StochasticShadows.DownsampledTileMask"));

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
		StochasticShadowsParameters.NumSamplesPerPixel = NumSamplesPerPixel2d;
		StochasticShadowsParameters.NumSamplesPerPixelDivideShift.X = FMath::FloorLog2(NumSamplesPerPixel2d.X);
		StochasticShadowsParameters.NumSamplesPerPixelDivideShift.Y = FMath::FloorLog2(NumSamplesPerPixel2d.Y);
		StochasticShadowsParameters.StochasticShadowsStateFrameIndex = StochasticShadows::GetStateFrameIndex(View.ViewState);
		StochasticShadowsParameters.DownsampledTileMask = DownsampledTileMask;
		StochasticShadowsParameters.DownsampledSceneDepth = DownsampledSceneDepth;
		StochasticShadowsParameters.DownsampledSceneWorldNormal = DownsampledSceneWorldNormal;
		StochasticShadowsParameters.MaxShadowMaskTiles = MaxShadowMaskTiles;
		StochasticShadowsParameters.MaxShadingTiles = (ShadingTileAtlasSize.X * ShadingTileAtlasSize.Y) / (StochasticShadows::ShadowMaskTileSize * StochasticShadows::ShadowMaskTileSize);
		StochasticShadowsParameters.MaxShadingTilesPerGridCell = StochasticShadows::MaxShadingTilesPerGridCell;
		StochasticShadowsParameters.ShadingTileGridSize = ShadingTileGridSize;
		StochasticShadowsParameters.ShadowMaskPageTablePerLightSize = ShadowMaskPageTablePerLightSize;
		StochasticShadowsParameters.DownsampledBufferInvSize = FVector2f(1.0f) / DownsampledBufferSize;
		StochasticShadowsParameters.SamplingMinWeight = FMath::Max(CVarStochasticShadowsSamplingMinWeight.GetValueOnRenderThread(), 0.0f);
		StochasticShadowsParameters.TileDataStride = TileDataStride;
		StochasticShadowsParameters.DownsampledTileDataStride = DownsampledTileDataStride;
		StochasticShadowsParameters.TemporalMaxFramesAccumulated = CVarStochasticShadowsTemporalMaxFramesAccumulated.GetValueOnRenderThread();
		StochasticShadowsParameters.TemporalStdDevOffset = CVarStochasticShadowsTemporalStdDevOffset.GetValueOnRenderThread();
		StochasticShadowsParameters.DebugMode = StochasticShadows::GetDebugMode();
		StochasticShadowsParameters.DebugLightId = INDEX_NONE;

		if (bDebug)
		{
			ShaderPrint::SetEnabled(true);
			ShaderPrint::RequestSpaceForLines(1024);
			ShaderPrint::SetParameters(GraphBuilder, View.ShaderPrintData, StochasticShadowsParameters.ShaderPrintUniformBuffer);

			StochasticShadowsParameters.DebugLightId = CVarStochasticShadowsDebugLightId.GetValueOnRenderThread();

			if (StochasticShadowsParameters.DebugLightId < 0)
			{
				for (auto LightIt = Scene->Lights.CreateConstIterator(); LightIt; ++LightIt)
				{
					const FLightSceneInfoCompact& LightSceneInfoCompact = *LightIt;
					const FLightSceneInfo* const LightSceneInfo = LightSceneInfoCompact.LightSceneInfo;

					if (LightSceneInfo->Proxy->IsSelected())
					{
						StochasticShadowsParameters.DebugLightId = LightSceneInfo->Id;
						break;
					}
				}
			}
		}
	}

	FRDGBufferRef TileAllocator = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), (int32)StochasticShadows::ETileType::MAX), TEXT("StochasticShadows.TileAllocator"));
	FRDGBufferRef TileData = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), TileDataStride * (int32)StochasticShadows::ETileType::MAX), TEXT("StochasticShadows.TileData"));
	AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(TileAllocator), 0);

	FRDGBufferRef DownsampledTileAllocator = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), (int32)StochasticShadows::ETileType::MAX), TEXT("StochasticShadows.DownsampledTileAllocator"));
	FRDGBufferRef DownsampledTileData = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), DownsampledTileDataStride * (int32)StochasticShadows::ETileType::MAX), TEXT("StochasticShadows.DownsampledTileData"));
	AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(DownsampledTileAllocator), 0);

	// #sdl_todo: merge classification passes or reuse downsampled one to create full res tiles
	// Run tile classification to generate tiles for the subsequent passes
	{
		{
			FTileClassificationCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FTileClassificationCS::FParameters>();
			PassParameters->StochasticShadowsParameters = StochasticShadowsParameters;
			PassParameters->RWTileAllocator = GraphBuilder.CreateUAV(TileAllocator);
			PassParameters->RWTileData = GraphBuilder.CreateUAV(TileData);
			PassParameters->RWDownsampledTileMask = nullptr;

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
			PassParameters->StochasticShadowsParameters = StochasticShadowsParameters;
			PassParameters->RWTileAllocator = GraphBuilder.CreateUAV(DownsampledTileAllocator);
			PassParameters->RWTileData = GraphBuilder.CreateUAV(DownsampledTileData);
			PassParameters->RWDownsampledTileMask = GraphBuilder.CreateUAV(DownsampledTileMask);

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

	FRDGBufferRef TileIndirectArgs = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateIndirectDesc<FRHIDispatchIndirectParameters>((int32)StochasticShadows::ETileType::MAX), TEXT("StochasticShadows.TileIndirectArgs"));
	FRDGBufferRef DownsampledTileIndirectArgs = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateIndirectDesc<FRHIDispatchIndirectParameters>((int32)StochasticShadows::ETileType::MAX), TEXT("StochasticShadows.DownsampledTileIndirectArgs"));

	// Setup indirect args for classified tiles
	{
		FInitTileIndirectArgsCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FInitTileIndirectArgsCS::FParameters>();
		PassParameters->StochasticShadowsParameters = StochasticShadowsParameters;
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

	FRDGTextureRef ShadowMaskHistoryWeights = GraphBuilder.CreateTexture(
		FRDGTextureDesc::Create2D(ShadowMaskBufferSize, PF_FloatRGBA, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV),
		TEXT("StochasticShadows.ShadowMaskHistoryWeights"));

	FRDGTextureRef ShadowMaskHistoryScreenCoord00 = GraphBuilder.CreateTexture(
		FRDGTextureDesc::Create2D(ShadowMaskBufferSize, PF_R16G16_UINT, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV),
		TEXT("StochasticShadows.ShadowMaskHistoryScreenCoord00"));

	FRDGBufferRef ShadowMaskTileAllocator = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), 1), TEXT("StochasticShadows.ShadowMaskTileAllocator"));
	FRDGBufferRef ShadowMaskTileData = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(2 * sizeof(uint32), MaxShadowMaskTiles), TEXT("StochasticShadows.ShadowMaskTileData"));
	AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(ShadowMaskTileAllocator), 0);
	
	// Generate new candidate light samples
	{
		FRDGTextureUAVRef DownsampledSceneDepthUAV = GraphBuilder.CreateUAV(DownsampledSceneDepth, ERDGUnorderedAccessViewFlags::SkipBarrier);
		FRDGTextureUAVRef DownsampledSceneWorldNormalUAV = GraphBuilder.CreateUAV(DownsampledSceneWorldNormal, ERDGUnorderedAccessViewFlags::SkipBarrier);
		FRDGTextureUAVRef ShadowMaskHistoryScreenCoord00UAV = GraphBuilder.CreateUAV(ShadowMaskHistoryScreenCoord00, ERDGUnorderedAccessViewFlags::SkipBarrier);
		FRDGTextureUAVRef ShadowMaskHistoryWeightsUAV = GraphBuilder.CreateUAV(ShadowMaskHistoryWeights, ERDGUnorderedAccessViewFlags::SkipBarrier);
		FRDGBufferUAVRef ShadowMaskTileAllocatorUAV = GraphBuilder.CreateUAV(ShadowMaskTileAllocator, ERDGUnorderedAccessViewFlags::SkipBarrier);
		FRDGBufferUAVRef ShadowMaskTileDataUAV = GraphBuilder.CreateUAV(ShadowMaskTileData, ERDGUnorderedAccessViewFlags::SkipBarrier);
		FRDGTextureUAVRef ShadowMaskPageTableUAV = ShadowMaskPageTable ? GraphBuilder.CreateUAV(ShadowMaskPageTable, ERDGUnorderedAccessViewFlags::SkipBarrier) : nullptr;
		FRDGTextureUAVRef LightSamplesUAV = GraphBuilder.CreateUAV(LightSamples, ERDGUnorderedAccessViewFlags::SkipBarrier);

		for (int32 TileType = 0; TileType < (int32)StochasticShadows::ETileType::MAX; ++TileType)
		{
			FGenerateSamplesCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FGenerateSamplesCS::FParameters>();
			PassParameters->IndirectArgs = DownsampledTileIndirectArgs;
			PassParameters->StochasticShadowsParameters = StochasticShadowsParameters;
			PassParameters->RWDownsampledSceneDepth = DownsampledSceneDepthUAV;
			PassParameters->RWDownsampledSceneWorldNormal = DownsampledSceneWorldNormalUAV;
			PassParameters->RWShadowMaskHistoryScreenCoord00 = ShadowMaskHistoryScreenCoord00UAV;
			PassParameters->RWShadowMaskHistoryWeights = ShadowMaskHistoryWeightsUAV;
			PassParameters->RWShadowMaskTileAllocator = ShadowMaskTileAllocatorUAV;
			PassParameters->RWShadowMaskTileData = ShadowMaskTileDataUAV;
			PassParameters->RWShadowMaskPageTable = ShadowMaskPageTableUAV;
			PassParameters->RWLightSamples = LightSamplesUAV;
			PassParameters->DownsampledTileAllocator = GraphBuilder.CreateSRV(DownsampledTileAllocator);
			PassParameters->DownsampledTileData = GraphBuilder.CreateSRV(DownsampledTileData);
			PassParameters->ForwardLightData = View.ForwardLightingResources.ForwardLightUniformBuffer;
			PassParameters->ShadowMaskPageTableHistory = ShadowMaskPageTableHistory;
			PassParameters->ShadowMaskAtlasHistory = ShadowMaskAtlasHistory;
			PassParameters->ShadowMaskTileAtlas = ShadowMaskTileAtlas;
			PassParameters->ShadowMaskSceneDepthHistory = ShadowMaskSceneDepthHistory;
			PassParameters->HistoryScreenPositionScaleBias = HistoryScreenPositionScaleBias;
			PassParameters->HistoryUVMinMax = HistoryUVMinMax;

			FGenerateSamplesCS::FPermutationDomain PermutationVector;
			PermutationVector.Set<FGenerateSamplesCS::FTileType>(TileType);
			PermutationVector.Set<FGenerateSamplesCS::FTexturedRectLights>(CVarStochasticShadowsTexturedRectLights.GetValueOnRenderThread() != 0);
			PermutationVector.Set<FGenerateSamplesCS::FNumSamplesPerPixel1d>(NumSamplesPerPixel2d.X * NumSamplesPerPixel2d.Y);
			PermutationVector.Set<FGenerateSamplesCS::FShadowMaskReprojectionWeights>(bTemporal);
			PermutationVector.Set<FGenerateSamplesCS::FShadowFactorEstimate>(bTemporal && CVarStochasticSamplingShadowEstimate.GetValueOnRenderThread() != 0);
			PermutationVector.Set<FGenerateSamplesCS::FDebugMode>(bDebug);
			PermutationVector.Set<FGenerateSamplesCS::FCandidateLightMask>(ShadowMaskAtlasHistory && CVarStochasticShadowsCandidateLightMask.GetValueOnRenderThread() != 0);
			PermutationVector = FGenerateSamplesCS::RemapPermutation(PermutationVector);
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

	StochasticShadows::RayTraceLightSamples(
		View,
		GraphBuilder, 
		SceneTextures,
		SampleBufferSize,
		LightSamples,
		LightSampleRayDistance,
		StochasticShadowsParameters
	);

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

	// Composite shadow masks traces
	{
		FCompositeShadowMaskTracesCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FCompositeShadowMaskTracesCS::FParameters>();
		PassParameters->StochasticShadowsParameters = StochasticShadowsParameters;
		PassParameters->IndirectArgs = ShadowMaskTileIndirectArgs;
		PassParameters->RWShadowMaskTileAtlas = GraphBuilder.CreateUAV(ShadowMaskTileAtlas);
		PassParameters->ShadowMaskPageTable = ShadowMaskPageTable;
		PassParameters->ShadowMaskTileData = GraphBuilder.CreateSRV(ShadowMaskTileData);
		PassParameters->ShadowMaskHistoryScreenCoord00 = ShadowMaskHistoryScreenCoord00;
		PassParameters->ShadowMaskHistoryWeights = ShadowMaskHistoryWeights;
		PassParameters->ShadowMaskPageTableHistory = ShadowMaskPageTableHistory;
		PassParameters->ShadowMaskAtlasHistory = ShadowMaskAtlasHistory;
		PassParameters->LightSamples = LightSamples;
		PassParameters->ForwardLightData = View.ForwardLightingResources.ForwardLightUniformBuffer;

		FCompositeShadowMaskTracesCS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FCompositeShadowMaskTracesCS::FTemporalAccumulation>(bTemporal);
		PermutationVector.Set<FCompositeShadowMaskTracesCS::FDebugMode>(bDebug);
		auto ComputeShader = View.ShaderMap->GetShader<FCompositeShadowMaskTracesCS>(PermutationVector);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("CompositeShadowMaskTraces"),
			ComputeShader,
			PassParameters,
			ShadowMaskTileIndirectArgs,
			0);
	}

	// Spatial denoising pass
	{
		FShadowMaskSpatialPassCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FShadowMaskSpatialPassCS::FParameters>();
		PassParameters->StochasticShadowsParameters = StochasticShadowsParameters;
		PassParameters->IndirectArgs = ShadowMaskTileIndirectArgs;
		PassParameters->RWShadingTileAllocator = GraphBuilder.CreateUAV(ShadingTileAllocator);
		PassParameters->RWShadingTileGridAllocator = GraphBuilder.CreateUAV(ShadingTileGridAllocator);
		PassParameters->RWShadingTileGrid = GraphBuilder.CreateUAV(ShadingTileGrid);
		PassParameters->RWShadingTileAtlas = GraphBuilder.CreateUAV(ShadingTileAtlas);
		PassParameters->ShadowMaskUpsampleWeights = ShadowMaskUpsampleWeights;
		PassParameters->ShadowMaskTileData = GraphBuilder.CreateSRV(ShadowMaskTileData);
		PassParameters->ShadowMaskPageTable = ShadowMaskPageTable;
		PassParameters->ShadowMaskTileAtlas = ShadowMaskTileAtlas;

		FShadowMaskSpatialPassCS::FPermutationDomain PermutationVector;
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

	// Shade light samples
	{
		FRDGTextureUAVRef SceneColorUAV = GraphBuilder.CreateUAV(SceneTextures.Color.Target, ERDGUnorderedAccessViewFlags::SkipBarrier);

		for (int32 TileType = 0; TileType < (int32)StochasticShadows::ETileType::MAX; ++TileType)
		{
			FShadeLightSamplesCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FShadeLightSamplesCS::FParameters>();
			PassParameters->IndirectArgs = TileIndirectArgs;
			PassParameters->StochasticShadowsParameters = StochasticShadowsParameters;
			PassParameters->ShadowMaskPageTable = ShadowMaskPageTable;
			PassParameters->ShadowMaskTileAtlas = ShadowMaskTileAtlas;
			PassParameters->ShadowMaskTileAllocator = GraphBuilder.CreateSRV(ShadowMaskTileAllocator);
			PassParameters->TileAllocator = GraphBuilder.CreateSRV(TileAllocator);
			PassParameters->TileData = GraphBuilder.CreateSRV(TileData);
			PassParameters->ShadingTileAllocator = GraphBuilder.CreateSRV(ShadingTileAllocator);
			PassParameters->ShadingTileGridAllocator = ShadingTileGridAllocator;
			PassParameters->ShadingTileGrid = GraphBuilder.CreateSRV(ShadingTileGrid);
			PassParameters->ShadingTileAtlas = ShadingTileAtlas;
			PassParameters->RWSceneColor = SceneColorUAV;

			FShadeLightSamplesCS::FPermutationDomain PermutationVector;
			PermutationVector.Set<FShadeLightSamplesCS::FTileType>(TileType);
			PermutationVector.Set<FShadeLightSamplesCS::FTexturedRectLights>(CVarStochasticShadowsTexturedRectLights.GetValueOnRenderThread() != 0);
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

	if (View.ViewState && !View.bStatePrevViewInfoIsReadOnly)
	{
		FStochasticShadowsViewState& LightingViewState = View.ViewState->StochasticShadows;
		LightingViewState.ShadowMaskPageTableHistory = nullptr;
		LightingViewState.ShadowMaskAtlasHistory = nullptr;
		LightingViewState.ShadowMaskSceneDepthHistory = nullptr;

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