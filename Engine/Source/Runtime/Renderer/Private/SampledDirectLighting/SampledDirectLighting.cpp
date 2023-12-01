// Copyright Epic Games, Inc. All Rights Reserved.

#include "SampledDirectLighting.h"
#include "SampledDirectLightingInternal.h"
#include "RendererPrivate.h"
#include "PixelShaderUtils.h"
#include "BasePassRendering.h"

static TAutoConsoleVariable<int32> CVarSampledDirectLighting(
	TEXT("r.SampledDirectLighting"),
	0,
	TEXT("Whether to enable Stochastic Direct Lighting. Experimental feature stochastically sampling analytical light in a single pass by leveraging ray tracing.\n")
	TEXT("1 - all lights using ray tracing shadows will be stochastically sampled\n")
	TEXT("2 - all lights will be stochastically sampled"),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarSampledDirectLightingNumSamplesPerPixel(
	TEXT("r.SampledDirectLighting.NumSamplesPerPixel"),
	4,
	TEXT("Number of samples (shadow rays) per half-res pixel.\n")
	TEXT("1 - 0.25 trace per pixel\n")
	TEXT("2 - 0.5 trace per pixel\n")
	TEXT("4 - 1 trace per pixel"),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarSampledDirectLightingMaxShadingTilesPerGridCell(
	TEXT("r.SampledDirectLighting.MaxShadingTilesPerGridCell"),
	32,
	TEXT("Maximum number of shading tiles per grid cell."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarSampledDirectLightingTemporal(
	TEXT("r.SampledDirectLighting.Temporal"),
	1,
	TEXT("Whether to use temporal accumulation for shadow mask."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarSampledDirectLightingTemporalMaxFramesAccumulated(
	TEXT("r.SampledDirectLighting.Temporal.MaxFramesAccumulated"),
	8,
	TEXT("Max history length when accumulating frames. Lower values have less ghosting, but more noise."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> CVarSampledDirectLightingTemporalStdDevOffset(
	TEXT("r.SampledDirectLighting.Temporal.StdDevOffset"),
	0.1f,
	TEXT("Increases standard deviation in neighborhood clamp. Higher values cause more ghosting, but allow smoother temporal accumulation."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> CVarSampledDirectLightingSamplingMinWeight(
	TEXT("r.SampledDirectLighting.Sampling.MinWeight"),
	0.002f,
	TEXT("Determines minimal sample influence on final pixels. Used to skip samples which would have minimal impact to the final image even if light is fully visible."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarSampledDirectLightingSpatial(
	TEXT("r.SampledDirectLighting.Spatial"),
	0,
	TEXT("Whether to run spatial shadow mask denoising pass."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarSampledDirectLightingWaveOps(
	TEXT("r.SampledDirectLighting.WaveOps"),
	1,
	TEXT("Whether to use wave ops. Useful for debugging."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarSampledDirectLightingDebug(
	TEXT("r.SampledDirectLighting.Debug"),
	0,
	TEXT("Whether to enabled debug mode, which prints various extra debug information from shaders.")
	TEXT("0 - Disable\n")
	TEXT("1 - Visualize sampling\n")
	TEXT("2 - Visualize tracing\n"),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarSampledDirectLightingDebugLightId(
	TEXT("r.SampledDirectLighting.Debug.LightId"),
	-1,
	TEXT("Which light to show debug info for. When set to -1, uses the currently selected light in editor."),
	ECVF_RenderThreadSafe
);

int32 GSampledDirectLightingReset = 0;
FAutoConsoleVariableRef CVarSampledDirectLightingReset(
	TEXT("r.SampledDirectLighting.Reset"),
	GSampledDirectLightingReset,
	TEXT("Reset history for debugging."),
	ECVF_RenderThreadSafe
);

int32 GSampledDirectLightingResetEveryNthFrame = 0;
	FAutoConsoleVariableRef CVarSampledDirectLightingResetEveryNthFrame(
	TEXT("r.SampledDirectLighting.ResetEveryNthFrame"),
		GSampledDirectLightingResetEveryNthFrame,
	TEXT("Reset history every Nth frame for debugging."),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int> CVarSampledDirectLightingFixedStateFrameIndex(
	TEXT("r.SampledDirectLighting.FixedStateFrameIndex"),
	-1,
	TEXT("Whether to override View.StateFrameIndex for debugging."),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int> CVarSampledDirectLightingCandidateLightMask(
	TEXT("r.SampledDirectLighting.CandidateLightMask"),
	1,
	TEXT("#sdl_todo: finish and pick one shader path."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int> CVarSampledDirectLightingTexturedRectLights(
	TEXT("r.SampledDirectLighting.TexturedRectLights"),
	0,
	TEXT("Whether to support textured rect lights."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

namespace SampledDirectLighting
{
	// must match values in SampledDirectLighting.ush
	constexpr int32 TileSize = 8;
	constexpr int32 MaxLocalLightIndexXY = 16; // 16 * 16 = 256
	constexpr uint32 ShadingTileIndexUnshadowed = 0xFFFFF; // limited by PackShadingTile()
	constexpr uint32 ShadingAtlasSizeInTiles = 512;

	bool IsEnabled()
	{
		return CVarSampledDirectLighting.GetValueOnRenderThread() != 0;
	}

	bool IsLightSupported(uint8 LightType, ECastRayTracedShadow::Type CastRayTracedShadow)
	{
		if (SampledDirectLighting::IsEnabled() && LightType != LightType_Directional)
		{
			const bool bRayTracedShadows = (CastRayTracedShadow == ECastRayTracedShadow::Enabled || (ShouldRenderRayTracingShadows() && CastRayTracedShadow == ECastRayTracedShadow::UseProjectSetting));
			return CVarSampledDirectLighting.GetValueOnRenderThread() == 2 || bRayTracedShadows;
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

		if (CVarSampledDirectLightingFixedStateFrameIndex.GetValueOnRenderThread() >= 0)
		{
			StateFrameIndex = CVarSampledDirectLightingFixedStateFrameIndex.GetValueOnRenderThread();
		}
		
		return StateFrameIndex;
	}

	FIntPoint GetNumSamplesPerPixel2d()
	{
		const uint32 NumSamplesPerPixel1d = FMath::RoundUpToPowerOfTwo(FMath::Clamp(CVarSampledDirectLightingNumSamplesPerPixel.GetValueOnRenderThread(), 1, 4));
		return NumSamplesPerPixel1d == 4 ? FIntPoint(2, 2) : (NumSamplesPerPixel1d == 2 ? FIntPoint(2, 1) : FIntPoint(1, 1));
	}

	int32 GetDebugMode()
	{
		return CVarSampledDirectLightingDebug.GetValueOnRenderThread();
	}

	bool UseWaveOps(EShaderPlatform ShaderPlatform)
	{
		return CVarSampledDirectLightingWaveOps.GetValueOnRenderThread() != 0 
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
		MAX = 2
	};
}

class FTileClassificationCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FTileClassificationCS)
	SHADER_USE_PARAMETER_STRUCT(FTileClassificationCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FSampledDirectLightingParameters, SampledDirectLightingParameters)
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
		return SampledDirectLighting::ShouldCompileShaders(Parameters);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());
	}
};

IMPLEMENT_GLOBAL_SHADER(FTileClassificationCS, "/Engine/Private/SampledDirectLighting/SampledDirectLighting.usf", "TileClassificationCS", SF_Compute);

class FInitTileIndirectArgsCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FInitTileIndirectArgsCS)
	SHADER_USE_PARAMETER_STRUCT(FInitTileIndirectArgsCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FSampledDirectLightingParameters, SampledDirectLightingParameters)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWTileIndirectArgs)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWDownsampledTileIndirectArgs)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, TileAllocator)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, DownsampledTileAllocator)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return SampledDirectLighting::ShouldCompileShaders(Parameters);
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

IMPLEMENT_GLOBAL_SHADER(FInitTileIndirectArgsCS, "/Engine/Private/SampledDirectLighting/SampledDirectLighting.usf", "InitTileIndirectArgsCS", SF_Compute);

class FInitCompositeIndirectArgsCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FInitCompositeIndirectArgsCS)
	SHADER_USE_PARAMETER_STRUCT(FInitCompositeIndirectArgsCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FSampledDirectLightingParameters, SampledDirectLightingParameters)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWIndirectArgs)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, CompositeTileAllocator)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return SampledDirectLighting::ShouldCompileShaders(Parameters);
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

IMPLEMENT_GLOBAL_SHADER(FInitCompositeIndirectArgsCS, "/Engine/Private/SampledDirectLighting/SampledDirectLighting.usf", "InitCompositeIndirectArgsCS", SF_Compute);

class FGenerateSamplesCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FGenerateSamplesCS)
	SHADER_USE_PARAMETER_STRUCT(FGenerateSamplesCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		RDG_BUFFER_ACCESS(IndirectArgs, ERHIAccess::IndirectArgs)
		SHADER_PARAMETER_STRUCT_INCLUDE(FSampledDirectLightingParameters, SampledDirectLightingParameters)
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

	class FTileType : SHADER_PERMUTATION_INT("TILE_TYPE", (int32)SampledDirectLighting::ETileType::MAX);
	class FTexturedRectLights : SHADER_PERMUTATION_BOOL("USE_SOURCE_TEXTURE");
	class FNumSamplesPerPixel1d : SHADER_PERMUTATION_SPARSE_INT("NUM_SAMPLES_PER_PIXEL_1D", 1, 2, 4);
	class FCandidateLightMask : SHADER_PERMUTATION_BOOL("CANDIDATE_LIGHT_MASK");
	class FDebugMode : SHADER_PERMUTATION_BOOL("DEBUG_MODE");
	using FPermutationDomain = TShaderPermutationDomain<FTileType, FTexturedRectLights, FNumSamplesPerPixel1d, FCandidateLightMask, FDebugMode>;

	static int32 GetGroupSize()
	{	
		return 8;
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return SampledDirectLighting::ShouldCompileShaders(Parameters);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		SampledDirectLighting::ModifyCompilationEnvironment(Parameters.Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());
	}
};

IMPLEMENT_GLOBAL_SHADER(FGenerateSamplesCS, "/Engine/Private/SampledDirectLighting/SampledDirectLightingSampling.usf", "GenerateSamplesCS", SF_Compute);

class FInitCompositeUpsampleWeightsCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FInitCompositeUpsampleWeightsCS)
	SHADER_USE_PARAMETER_STRUCT(FInitCompositeUpsampleWeightsCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FSampledDirectLightingParameters, SampledDirectLightingParameters)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, RWCompositeUpsampleWeights)
	END_SHADER_PARAMETER_STRUCT()

	static int32 GetGroupSize()
	{
		return 8;
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return SampledDirectLighting::ShouldCompileShaders(Parameters);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());
	}
};

IMPLEMENT_GLOBAL_SHADER(FInitCompositeUpsampleWeightsCS, "/Engine/Private/SampledDirectLighting/SampledDirectLighting.usf", "InitCompositeUpsampleWeightsCS", SF_Compute);

class FCompositeLightSamplesCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FCompositeLightSamplesCS)
	SHADER_USE_PARAMETER_STRUCT(FCompositeLightSamplesCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FSampledDirectLightingParameters, SampledDirectLightingParameters)
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
		return SampledDirectLighting::ShouldCompileShaders(Parameters);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		SampledDirectLighting::ModifyCompilationEnvironment(Parameters.Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());
	}
};

IMPLEMENT_GLOBAL_SHADER(FCompositeLightSamplesCS, "/Engine/Private/SampledDirectLighting/SampledDirectLightingComposite.usf", "CompositeLightSamplesCS", SF_Compute);

class FShadeLightSamplesCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FShadeLightSamplesCS)
	SHADER_USE_PARAMETER_STRUCT(FShadeLightSamplesCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		RDG_BUFFER_ACCESS(IndirectArgs, ERHIAccess::IndirectArgs)
		SHADER_PARAMETER_STRUCT_INCLUDE(FSampledDirectLightingParameters, SampledDirectLightingParameters)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float3>, RWDiffuseLighting)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float3>, RWSpecularLighting)
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

	class FTileType : SHADER_PERMUTATION_INT("TILE_TYPE", (int32)SampledDirectLighting::ETileType::MAX);
	class FTexturedRectLights : SHADER_PERMUTATION_BOOL("USE_SOURCE_TEXTURE");
	class FDebugMode : SHADER_PERMUTATION_BOOL("DEBUG_MODE");
	using FPermutationDomain = TShaderPermutationDomain<FTileType, FTexturedRectLights, FDebugMode>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return SampledDirectLighting::ShouldCompileShaders(Parameters);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		SampledDirectLighting::ModifyCompilationEnvironment(Parameters.Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());
	}
};

IMPLEMENT_GLOBAL_SHADER(FShadeLightSamplesCS, "/Engine/Private/SampledDirectLighting/SampledDirectLightingShading.usf", "ShadeLightSamplesCS", SF_Compute);

class FSampledDirectLightingTemporalAccumulationCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FSampledDirectLightingTemporalAccumulationCS)
	SHADER_USE_PARAMETER_STRUCT(FSampledDirectLightingTemporalAccumulationCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FSampledDirectLightingParameters, SampledDirectLightingParameters)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float4>, DiffuseLightingTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float4>, SpecularLightingTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float4>, DiffuseLightingHistoryTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float4>, SpecularLightingHistoryTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float>, SampledDirectLightingDepthHistory)
		SHADER_PARAMETER(FVector4f, HistoryUVMinMax)
		SHADER_PARAMETER(FVector4f, HistoryScreenPositionScaleBias)
		SHADER_PARAMETER(float, PrevSceneColorPreExposureCorrection)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float3>, RWDiffuseLightingTexture)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float3>, RWSpecularLightingTexture)
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
		return SampledDirectLighting::ShouldCompileShaders(Parameters);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());
	}
};

IMPLEMENT_GLOBAL_SHADER(FSampledDirectLightingTemporalAccumulationCS, "/Engine/Private/SampledDirectLighting/SampledDirectLightingDenoising.usf", "SampledDirectLightingTemporalAccumulationCS", SF_Compute);

/**
 * Single pass batched light rendering using ray tracing (distance field or triangle) for shadowing.
 */
void FDeferredShadingSceneRenderer::RenderSampledDirectLighting(FRDGBuilder& GraphBuilder, const FSceneTextures& SceneTextures)
{
	if (!SampledDirectLighting::IsEnabled())
	{
		return;
	}

	check(AreLightsInLightGrid());

	RDG_EVENT_SCOPE(GraphBuilder, "SampledDirectLighting");

	const FViewInfo& View = Views[0];
	FBlueNoise BlueNoise = GetBlueNoiseGlobalParameters();
	TUniformBufferRef<FBlueNoise> BlueNoiseUniformBuffer = CreateUniformBufferImmediate(BlueNoise, EUniformBufferUsage::UniformBuffer_SingleDraw);

	const bool bDebug = SampledDirectLighting::GetDebugMode() != 0;
	const bool bWaveOps = SampledDirectLighting::UseWaveOps(View.GetShaderPlatform())
		&& GRHIMinimumWaveSize <= 32
		&& GRHIMaximumWaveSize >= 32;

	// History reset for debugging purposes
	bool bResetHistory = false;

	if (GSampledDirectLightingResetEveryNthFrame > 0 && (ViewFamily.FrameNumber % (uint32)GSampledDirectLightingResetEveryNthFrame) == 0)
	{
		bResetHistory = true;
	}

	if (GSampledDirectLightingReset != 0)
	{
		GSampledDirectLightingReset = 0;
		bResetHistory = true;
	}

	const FIntPoint NumSamplesPerPixel2d = SampledDirectLighting::GetNumSamplesPerPixel2d();

	const uint32 DownsampleFactor = 2;
	const FIntPoint DownsampledViewSize = FIntPoint::DivideAndRoundUp(View.ViewRect.Size(), DownsampleFactor);
	const FIntPoint SampleViewSize = DownsampledViewSize * NumSamplesPerPixel2d;
	const FIntPoint DownsampledBufferSize = FIntPoint::DivideAndRoundUp(SceneTextures.Config.Extent, DownsampleFactor);
	const FIntPoint SampleBufferSize = DownsampledBufferSize * NumSamplesPerPixel2d;
	const FIntPoint DonwnsampledSampleBufferSize = DownsampledBufferSize * NumSamplesPerPixel2d;

	// #sdl_todo: make atlas size based on the resolution
	const FIntPoint ShadingTileGridSize = FIntPoint::DivideAndRoundUp(SceneTextures.Config.Extent, SampledDirectLighting::TileSize);
	const FIntPoint ShadingTileAtlasSize = SampledDirectLighting::TileSize * SampledDirectLighting::ShadingAtlasSizeInTiles;
	const int32 MaxShadingTiles = SampledDirectLighting::ShadingAtlasSizeInTiles * SampledDirectLighting::ShadingAtlasSizeInTiles;
	check(MaxShadingTiles == (ShadingTileAtlasSize.X * ShadingTileAtlasSize.Y) / (SampledDirectLighting::TileSize * SampledDirectLighting::TileSize));
	check(MaxShadingTiles < SampledDirectLighting::ShadingTileIndexUnshadowed);
	const int32 MaxCompositeTiles = MaxShadingTiles;

	const int32 MaxShadingTilesPerGridCell = FMath::Max(CVarSampledDirectLightingMaxShadingTilesPerGridCell.GetValueOnRenderThread(), 0);

	FRDGTextureRef DownsampledSceneDepth = GraphBuilder.CreateTexture(
		FRDGTextureDesc::Create2D(DownsampledBufferSize, PF_R32_FLOAT, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV),
		TEXT("SampledDirectLighting.DownsampledSceneDepth"));

	FRDGTextureRef DownsampledSceneWorldNormal = GraphBuilder.CreateTexture(
		FRDGTextureDesc::Create2D(DownsampledBufferSize, PF_A2B10G10R10, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV),
		TEXT("SampledDirectLighting.DownsampledSceneWorldNormal"));

	FRDGTextureRef LightSamples = GraphBuilder.CreateTexture(
		FRDGTextureDesc::Create2D(DonwnsampledSampleBufferSize, PF_R32_UINT, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV),
		TEXT("SampledDirectLighting.LightSamples"));

	FRDGTextureRef LightSampleRayDistance = GraphBuilder.CreateTexture(
		FRDGTextureDesc::Create2D(DonwnsampledSampleBufferSize, PF_R16F, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV),
		TEXT("SampledDirectLighting.LightSampleRayDistance"));
	
	bool bTemporal = CVarSampledDirectLightingTemporal.GetValueOnRenderThread() != 0;
	FVector4f HistoryScreenPositionScaleBias = FVector4f(0.0f, 0.0f, 0.0f, 0.0f);
	FVector4f HistoryUVMinMax = FVector4f(0.0f, 0.0f, 0.0f, 0.0f);
	FRDGTextureRef DiffuseLightingHistory = nullptr;
	FRDGTextureRef SpecularLightingHistory = nullptr;
	FRDGTextureRef SceneDepthHistory = nullptr;

	if (View.ViewState)
	{
		const FSampledDirectLightingViewState& LightingViewState = View.ViewState->SampledDirectLighting;

		if (!View.bCameraCut && !bResetHistory && bTemporal)
		{
			HistoryScreenPositionScaleBias = LightingViewState.HistoryScreenPositionScaleBias;
			HistoryUVMinMax = LightingViewState.HistoryUVMinMax;

			if (LightingViewState.DiffuseLightingHistory
				&& LightingViewState.SpecularLightingHistory
				&& LightingViewState.SceneDepthHistory
				&& LightingViewState.DiffuseLightingHistory->GetDesc().Extent == View.GetSceneTexturesConfig().Extent
				&& LightingViewState.SpecularLightingHistory->GetDesc().Extent == View.GetSceneTexturesConfig().Extent
				&& LightingViewState.SceneDepthHistory->GetDesc().Extent == SceneTextures.Depth.Resolve->Desc.Extent)
			{
				DiffuseLightingHistory = GraphBuilder.RegisterExternalTexture(LightingViewState.DiffuseLightingHistory);
				SpecularLightingHistory = GraphBuilder.RegisterExternalTexture(LightingViewState.SpecularLightingHistory);
				SceneDepthHistory = GraphBuilder.RegisterExternalTexture(LightingViewState.SceneDepthHistory);
			}
		}
	}

	const FIntPoint ViewSizeInTiles = FIntPoint::DivideAndRoundUp(View.ViewRect.Size(), SampledDirectLighting::TileSize);
	const int32 TileDataStride = ViewSizeInTiles.X * ViewSizeInTiles.Y;

	const FIntPoint DownsampledViewSizeInTiles = FIntPoint::DivideAndRoundUp(DownsampledViewSize, SampledDirectLighting::TileSize);
	const int32 DownsampledTileDataStride = DownsampledViewSizeInTiles.X * DownsampledViewSizeInTiles.Y;

	FRDGTextureRef DownsampledTileMask = GraphBuilder.CreateTexture(
		FRDGTextureDesc::Create2D(FMath::DivideAndRoundUp<FIntPoint>(DownsampledBufferSize, SampledDirectLighting::TileSize), PF_R8_UINT, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV),
		TEXT("SampledDirectLighting.DownsampledTileMask"));

	FSampledDirectLightingParameters SampledDirectLightingParameters;
	{
		SampledDirectLightingParameters.ViewUniformBuffer = View.ViewUniformBuffer;
		SampledDirectLightingParameters.Scene = View.GetSceneUniforms().GetBuffer(GraphBuilder);
		SampledDirectLightingParameters.SceneTextures = GetSceneTextureParameters(GraphBuilder, SceneTextures.UniformBuffer);
		SampledDirectLightingParameters.SceneTexturesStruct = SceneTextures.UniformBuffer;
		SampledDirectLightingParameters.Substrate = Substrate::BindSubstrateGlobalUniformParameters(View);
		SampledDirectLightingParameters.ForwardLightData = View.ForwardLightingResources.ForwardLightUniformBuffer;
		SampledDirectLightingParameters.BlueNoise = BlueNoiseUniformBuffer;
		SampledDirectLightingParameters.PreIntegratedGF = GSystemTextures.PreintegratedGF->GetRHI();
		SampledDirectLightingParameters.PreIntegratedGFSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
		SampledDirectLightingParameters.DownsampledViewSize = DownsampledViewSize;
		SampledDirectLightingParameters.SampleViewSize = SampleViewSize;
		SampledDirectLightingParameters.NumSamplesPerPixel = NumSamplesPerPixel2d;
		SampledDirectLightingParameters.NumSamplesPerPixelDivideShift.X = FMath::FloorLog2(NumSamplesPerPixel2d.X);
		SampledDirectLightingParameters.NumSamplesPerPixelDivideShift.Y = FMath::FloorLog2(NumSamplesPerPixel2d.Y);
		SampledDirectLightingParameters.SampledDirectLightingStateFrameIndex = SampledDirectLighting::GetStateFrameIndex(View.ViewState);
		SampledDirectLightingParameters.DownsampledTileMask = DownsampledTileMask;
		SampledDirectLightingParameters.DownsampledSceneDepth = DownsampledSceneDepth;
		SampledDirectLightingParameters.DownsampledSceneWorldNormal = DownsampledSceneWorldNormal;
		SampledDirectLightingParameters.MaxCompositeTiles = MaxCompositeTiles;
		SampledDirectLightingParameters.MaxShadingTiles = MaxShadingTiles;
		SampledDirectLightingParameters.MaxShadingTilesPerGridCell = MaxShadingTilesPerGridCell;
		SampledDirectLightingParameters.ShadingTileGridSize = ShadingTileGridSize;
		SampledDirectLightingParameters.DownsampledBufferInvSize = FVector2f(1.0f) / DownsampledBufferSize;
		SampledDirectLightingParameters.SamplingMinWeight = FMath::Max(CVarSampledDirectLightingSamplingMinWeight.GetValueOnRenderThread(), 0.0f);
		SampledDirectLightingParameters.TileDataStride = TileDataStride;
		SampledDirectLightingParameters.DownsampledTileDataStride = DownsampledTileDataStride;
		SampledDirectLightingParameters.TemporalMaxFramesAccumulated = CVarSampledDirectLightingTemporalMaxFramesAccumulated.GetValueOnRenderThread();
		SampledDirectLightingParameters.TemporalStdDevOffset = CVarSampledDirectLightingTemporalStdDevOffset.GetValueOnRenderThread();
		SampledDirectLightingParameters.DebugMode = SampledDirectLighting::GetDebugMode();
		SampledDirectLightingParameters.DebugLightId = INDEX_NONE;

		if (bDebug)
		{
			ShaderPrint::SetEnabled(true);
			ShaderPrint::RequestSpaceForLines(1024);
			ShaderPrint::SetParameters(GraphBuilder, View.ShaderPrintData, SampledDirectLightingParameters.ShaderPrintUniformBuffer);

			SampledDirectLightingParameters.DebugLightId = CVarSampledDirectLightingDebugLightId.GetValueOnRenderThread();

			if (SampledDirectLightingParameters.DebugLightId < 0)
			{
				for (auto LightIt = Scene->Lights.CreateConstIterator(); LightIt; ++LightIt)
				{
					const FLightSceneInfoCompact& LightSceneInfoCompact = *LightIt;
					const FLightSceneInfo* const LightSceneInfo = LightSceneInfoCompact.LightSceneInfo;

					if (LightSceneInfo->Proxy->IsSelected())
					{
						SampledDirectLightingParameters.DebugLightId = LightSceneInfo->Id;
						break;
					}
				}
			}
		}
	}

	FRDGBufferRef TileAllocator = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), (int32)SampledDirectLighting::ETileType::MAX), TEXT("SampledDirectLighting.TileAllocator"));
	FRDGBufferRef TileData = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), TileDataStride * (int32)SampledDirectLighting::ETileType::MAX), TEXT("SampledDirectLighting.TileData"));
	AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(TileAllocator), 0);

	FRDGBufferRef DownsampledTileAllocator = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), (int32)SampledDirectLighting::ETileType::MAX), TEXT("SampledDirectLighting.DownsampledTileAllocator"));
	FRDGBufferRef DownsampledTileData = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), DownsampledTileDataStride * (int32)SampledDirectLighting::ETileType::MAX), TEXT("SampledDirectLighting.DownsampledTileData"));
	AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(DownsampledTileAllocator), 0);

	// #sdl_todo: merge classification passes or reuse downsampled one to create full res tiles
	// Run tile classification to generate tiles for the subsequent passes
	{
		{
			FTileClassificationCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FTileClassificationCS::FParameters>();
			PassParameters->SampledDirectLightingParameters = SampledDirectLightingParameters;
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
			PassParameters->SampledDirectLightingParameters = SampledDirectLightingParameters;
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

	FRDGBufferRef TileIndirectArgs = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateIndirectDesc<FRHIDispatchIndirectParameters>((int32)SampledDirectLighting::ETileType::MAX), TEXT("SampledDirectLighting.TileIndirectArgs"));
	FRDGBufferRef DownsampledTileIndirectArgs = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateIndirectDesc<FRHIDispatchIndirectParameters>((int32)SampledDirectLighting::ETileType::MAX), TEXT("SampledDirectLighting.DownsampledTileIndirectArgs"));

	// Setup indirect args for classified tiles
	{
		FInitTileIndirectArgsCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FInitTileIndirectArgsCS::FParameters>();
		PassParameters->SampledDirectLightingParameters = SampledDirectLightingParameters;
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

	FRDGBufferRef CompositeTileAllocator = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), 1), TEXT("SampledDirectLighting.CompositeTileAllocator"));
	FRDGBufferRef CompositeTileData = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(2 * sizeof(uint32), MaxCompositeTiles), TEXT("SampledDirectLighting.CompositeTileData"));
	AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(CompositeTileAllocator), 0);
	
	// Generate new candidate light samples
	{
		FRDGTextureUAVRef DownsampledSceneDepthUAV = GraphBuilder.CreateUAV(DownsampledSceneDepth, ERDGUnorderedAccessViewFlags::SkipBarrier);
		FRDGTextureUAVRef DownsampledSceneWorldNormalUAV = GraphBuilder.CreateUAV(DownsampledSceneWorldNormal, ERDGUnorderedAccessViewFlags::SkipBarrier);
		FRDGBufferUAVRef CompositeTileAllocatorUAV = GraphBuilder.CreateUAV(CompositeTileAllocator, ERDGUnorderedAccessViewFlags::SkipBarrier);
		FRDGBufferUAVRef CompositeTileDataUAV = GraphBuilder.CreateUAV(CompositeTileData, ERDGUnorderedAccessViewFlags::SkipBarrier);
		FRDGTextureUAVRef LightSamplesUAV = GraphBuilder.CreateUAV(LightSamples, ERDGUnorderedAccessViewFlags::SkipBarrier);

		for (int32 TileType = 0; TileType < (int32)SampledDirectLighting::ETileType::MAX; ++TileType)
		{
			FGenerateSamplesCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FGenerateSamplesCS::FParameters>();
			PassParameters->IndirectArgs = DownsampledTileIndirectArgs;
			PassParameters->SampledDirectLightingParameters = SampledDirectLightingParameters;
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
			PermutationVector.Set<FGenerateSamplesCS::FTexturedRectLights>(CVarSampledDirectLightingTexturedRectLights.GetValueOnRenderThread() != 0);
			PermutationVector.Set<FGenerateSamplesCS::FNumSamplesPerPixel1d>(NumSamplesPerPixel2d.X * NumSamplesPerPixel2d.Y);
			PermutationVector.Set<FGenerateSamplesCS::FDebugMode>(bDebug);
			PermutationVector.Set<FGenerateSamplesCS::FCandidateLightMask>(CVarSampledDirectLightingCandidateLightMask.GetValueOnRenderThread() != 0);
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

	FRDGBufferRef CompositeTileIndirectArgs = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateIndirectDesc<FRHIDispatchIndirectParameters>(1), TEXT("SampledDirectLighting.CompositeTileIndirectArgs"));

	// Setup indirect args for shadow mask tile updates
	{
		FInitCompositeIndirectArgsCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FInitCompositeIndirectArgsCS::FParameters>();
		PassParameters->SampledDirectLightingParameters = SampledDirectLightingParameters;
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

	SampledDirectLighting::RayTraceLightSamples(
		View,
		GraphBuilder, 
		SceneTextures,
		SampleBufferSize,
		LightSamples,
		LightSampleRayDistance,
		SampledDirectLightingParameters
	);

	FRDGTextureRef CompositeUpsampleWeights = GraphBuilder.CreateTexture(
		FRDGTextureDesc::Create2D(SceneTextures.Config.Extent, PF_FloatRGBA, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV),
		TEXT("SampledDirectLighting.CompositeUpsampleWeights"));

	// Init composite upsample weights
	{
		FInitCompositeUpsampleWeightsCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FInitCompositeUpsampleWeightsCS::FParameters>();
		PassParameters->SampledDirectLightingParameters = SampledDirectLightingParameters;
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
		TEXT("SampledDirectLightingParameters.ShadingTileAllocator"));

	FRDGTextureRef ShadingTileGridAllocator = GraphBuilder.CreateTexture(
		FRDGTextureDesc::Create2D(ShadingTileGridSize, PF_R32_UINT, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV),
		TEXT("SampledDirectLighting.ShadingTileGridAllocator"));

	FRDGBufferRef ShadingTileGrid = GraphBuilder.CreateBuffer(
		FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), ShadingTileGridSize.X * ShadingTileGridSize.Y * MaxShadingTilesPerGridCell),
		TEXT("SampledDirectLightingParameters.ShadingTileGrid"));

	FRDGTextureRef ShadingTileAtlas = GraphBuilder.CreateTexture(
		FRDGTextureDesc::Create2D(ShadingTileAtlasSize, PF_R16F, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV),
		TEXT("SampledDirectLighting.ShadingTileAtlas"));

	AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(ShadingTileAllocator), 0u);
	AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(ShadingTileGridAllocator), 0u);

	// Composite shadow masks traces
	{
		FCompositeLightSamplesCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FCompositeLightSamplesCS::FParameters>();
		PassParameters->SampledDirectLightingParameters = SampledDirectLightingParameters;
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
		TEXT("SampledDirectLighting.ResolvedDiffuseLighting"));

	FRDGTextureRef ResolvedSpecularLighting = GraphBuilder.CreateTexture(
		FRDGTextureDesc::Create2D(View.GetSceneTexturesConfig().Extent, PF_FloatRGB, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV),
		TEXT("SampledDirectLighting.ResolvedSpecularLighting"));

	// Shade light samples
	{
		FRDGTextureUAVRef SceneColorUAV = GraphBuilder.CreateUAV(SceneTextures.Color.Target, ERDGUnorderedAccessViewFlags::SkipBarrier);

		for (int32 TileType = 0; TileType < (int32)SampledDirectLighting::ETileType::MAX; ++TileType)
		{
			FShadeLightSamplesCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FShadeLightSamplesCS::FParameters>();
			PassParameters->RWDiffuseLighting = GraphBuilder.CreateUAV(ResolvedDiffuseLighting);
			PassParameters->RWSpecularLighting = GraphBuilder.CreateUAV(ResolvedSpecularLighting);
			PassParameters->IndirectArgs = TileIndirectArgs;
			PassParameters->SampledDirectLightingParameters = SampledDirectLightingParameters;
			PassParameters->CompositeTileAllocator = GraphBuilder.CreateSRV(CompositeTileAllocator);
			PassParameters->TileAllocator = GraphBuilder.CreateSRV(TileAllocator);
			PassParameters->TileData = GraphBuilder.CreateSRV(TileData);
			PassParameters->ShadingTileAllocator = GraphBuilder.CreateSRV(ShadingTileAllocator);
			PassParameters->ShadingTileGridAllocator = ShadingTileGridAllocator;
			PassParameters->ShadingTileGrid = GraphBuilder.CreateSRV(ShadingTileGrid);
			PassParameters->ShadingTileAtlas = ShadingTileAtlas;

			FShadeLightSamplesCS::FPermutationDomain PermutationVector;
			PermutationVector.Set<FShadeLightSamplesCS::FTileType>(TileType);
			PermutationVector.Set<FShadeLightSamplesCS::FTexturedRectLights>(CVarSampledDirectLightingTexturedRectLights.GetValueOnRenderThread() != 0);
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

	// Final diffuse and specular which will be passed to the next frame
	FRDGTextureRef DiffuseLighting = GraphBuilder.CreateTexture(
		FRDGTextureDesc::Create2D(View.GetSceneTexturesConfig().Extent, PF_FloatRGBA, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV),
		TEXT("SampledDirectLighting.DiffuseLighting"));

	FRDGTextureRef SpecularLighting = GraphBuilder.CreateTexture(
		FRDGTextureDesc::Create2D(View.GetSceneTexturesConfig().Extent, PF_FloatRGBA, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV),
		TEXT("SampledDirectLighting.SpecularLighting"));

	FRDGTextureRef SceneDepthCopy = GraphBuilder.CreateTexture(
		FRDGTextureDesc::Create2D(SceneTextures.Depth.Resolve->Desc.Extent, PF_R32_FLOAT, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV),
		TEXT("SampledDirectLighting.DepthHistory"));

	// Temporal accumulation
	{
		FSampledDirectLightingTemporalAccumulationCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FSampledDirectLightingTemporalAccumulationCS::FParameters>();
		PassParameters->SampledDirectLightingParameters = SampledDirectLightingParameters;
		PassParameters->DiffuseLightingTexture = ResolvedDiffuseLighting;
		PassParameters->SpecularLightingTexture = ResolvedSpecularLighting;
		PassParameters->DiffuseLightingHistoryTexture = DiffuseLightingHistory;
		PassParameters->SpecularLightingHistoryTexture = SpecularLightingHistory;
		PassParameters->SampledDirectLightingDepthHistory = SceneDepthHistory;
		PassParameters->PrevSceneColorPreExposureCorrection = View.PreExposure / View.PrevViewInfo.SceneColorPreExposure;
		PassParameters->HistoryScreenPositionScaleBias = HistoryScreenPositionScaleBias;
		PassParameters->HistoryUVMinMax = HistoryUVMinMax;
		PassParameters->RWDiffuseLightingTexture = GraphBuilder.CreateUAV(DiffuseLighting);
		PassParameters->RWSpecularLightingTexture = GraphBuilder.CreateUAV(SpecularLighting);
		PassParameters->RWSceneDepth = GraphBuilder.CreateUAV(SceneDepthCopy);
		PassParameters->RWSceneColor = GraphBuilder.CreateUAV(SceneTextures.Color.Target);

		FSampledDirectLightingTemporalAccumulationCS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FSampledDirectLightingTemporalAccumulationCS::FValidHistory>(DiffuseLightingHistory != nullptr && bTemporal);
		PermutationVector.Set<FSampledDirectLightingTemporalAccumulationCS::FDebugMode>(bDebug);
		auto ComputeShader = View.ShaderMap->GetShader<FSampledDirectLightingTemporalAccumulationCS>(PermutationVector);

		const FIntVector GroupCount = FComputeShaderUtils::GetGroupCount(View.ViewRect.Size(), FSampledDirectLightingTemporalAccumulationCS::GetGroupSize());

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("TemporalAccumulation"),
			ComputeShader,
			PassParameters,
			GroupCount);
	}

	if (View.ViewState && !View.bStatePrevViewInfoIsReadOnly)
	{
		FSampledDirectLightingViewState& LightingViewState = View.ViewState->SampledDirectLighting;

		LightingViewState.HistoryScreenPositionScaleBias = View.GetScreenPositionScaleBias(View.GetSceneTexturesConfig().Extent, View.ViewRect);

		// Pull in the max UV to exclude the region which will read outside the viewport due to bilinear filtering
		const FVector2D InvBufferSize(1.0f / SceneTextures.Config.Extent.X, 1.0f / SceneTextures.Config.Extent.Y);
		LightingViewState.HistoryUVMinMax = FVector4f(
			(View.ViewRect.Min.X + 0.5f) * InvBufferSize.X,
			(View.ViewRect.Min.Y + 0.5f) * InvBufferSize.Y,
			(View.ViewRect.Max.X - 1.0f) * InvBufferSize.X,
			(View.ViewRect.Max.Y - 1.0f) * InvBufferSize.Y);

		if (DiffuseLighting && SpecularLighting && SceneDepthCopy && bTemporal)
		{
			GraphBuilder.QueueTextureExtraction(DiffuseLighting, &LightingViewState.DiffuseLightingHistory);
			GraphBuilder.QueueTextureExtraction(SpecularLighting, &LightingViewState.SpecularLightingHistory);
			GraphBuilder.QueueTextureExtraction(SceneDepthCopy, &LightingViewState.SceneDepthHistory);
		}
		else
		{
			LightingViewState.DiffuseLightingHistory = nullptr;
			LightingViewState.SpecularLightingHistory = nullptr;
			LightingViewState.SceneDepthHistory = nullptr;
		}
	}
}