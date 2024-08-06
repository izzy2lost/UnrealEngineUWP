// Copyright Epic Games, Inc. All Rights Reserved.

#include "MegaLights.h"
#include "MegaLightsInternal.h"
#include "RendererPrivate.h"
#include "PixelShaderUtils.h"
#include "BasePassRendering.h"

static TAutoConsoleVariable<int32> CVarMegaLights(
	TEXT("r.MegaLights"),
	0,
	TEXT("Whether to enable Mega Lights. Experimental feature leveraging ray tracing to stochastically importance sample lights.\n")
	TEXT("1 - all lights using ray tracing shadows will be stochastically sampled\n")
	TEXT("2 - all lights will be stochastically sampled"),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarMegaLightsNumSamplesPerPixel(
	TEXT("r.MegaLights.NumSamplesPerPixel"),
	4,
	TEXT("Number of samples (shadow rays) per half-res pixel.\n")
	TEXT("2 - 0.5 trace per pixel\n")
	TEXT("4 - 1 trace per pixel")
	TEXT("16 - 4 traces per pixel"),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> CVarMegaLightsSamplingMinWeight(
	TEXT("r.MegaLights.Sampling.MinWeight"),
	0.001f,
	TEXT("Determines minimal sample influence on final pixels. Used to skip samples which would have minimal impact to the final image even if light is fully visible."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarMegaLightsTemporal(
	TEXT("r.MegaLights.Temporal"),
	1,
	TEXT("Whether to use temporal accumulation for shadow mask."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarMegaLightsTemporalMaxFramesAccumulated(
	TEXT("r.MegaLights.Temporal.MaxFramesAccumulated"),
	12,
	TEXT("Max history length when accumulating frames. Lower values have less ghosting, but more noise."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> CVarMegaLightsTemporalNeighborhoodClampScale(
	TEXT("r.MegaLights.Temporal.NeighborhoodClampScale"),
	2.0f,
	TEXT("Scales how permissive is neighborhood clamp. Higher values reduce noise, but also increase ghosting."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarMegaLightsSpatial(
	TEXT("r.MegaLights.Spatial"),
	1,
	TEXT("Whether denoiser should run spatial filter."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> CVarMegaLightsSpatialDepthWeightScale(
	TEXT("r.MegaLights.Spatial.DepthWeightScale"),
	10000.0f,
	TEXT("Scales the depth weight of the spatial filter. Smaller values allow for more sample reuse, but also introduce more bluriness between unrelated surfaces."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> CVarMegaLightsSpatialKernelRadius(
	TEXT("r.MegaLights.Spatial.KernelRadius"),
	8.0f,
	TEXT("Spatial filter kernel radius in pixels"),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarMegaLightsSpatialNumSamples(
	TEXT("r.MegaLights.Spatial.NumSamples"),
	4,
	TEXT("Number of spatial filter samples."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarMegaLightsWaveOps(
	TEXT("r.MegaLights.WaveOps"),
	1,
	TEXT("Whether to use wave ops. Useful for debugging."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarMegaLightsDebug(
	TEXT("r.MegaLights.Debug"),
	0,
	TEXT("Whether to enabled debug mode, which prints various extra debug information from shaders.")
	TEXT("0 - Disable\n")
	TEXT("1 - Visualize sampling\n")
	TEXT("2 - Visualize tracing\n"),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarMegaLightsDebugLightId(
	TEXT("r.MegaLights.Debug.LightId"),
	-1,
	TEXT("Which light to show debug info for. When set to -1, uses the currently selected light in editor."),
	ECVF_RenderThreadSafe
);

int32 GMegaLightsReset = 0;
FAutoConsoleVariableRef CVarMegaLightsReset(
	TEXT("r.MegaLights.Reset"),
	GMegaLightsReset,
	TEXT("Reset history for debugging."),
	ECVF_RenderThreadSafe
);

int32 GMegaLightsResetEveryNthFrame = 0;
	FAutoConsoleVariableRef CVarMegaLightsResetEveryNthFrame(
	TEXT("r.MegaLights.ResetEveryNthFrame"),
		GMegaLightsResetEveryNthFrame,
	TEXT("Reset history every Nth frame for debugging."),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarMegaLightsFixedStateFrameIndex(
	TEXT("r.MegaLights.FixedStateFrameIndex"),
	-1,
	TEXT("Whether to override View.StateFrameIndex for debugging."),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarMegaLightsTexturedRectLights(
	TEXT("r.MegaLights.TexturedRectLights"),
	0,
	TEXT("Whether to support textured rect lights."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarMegaLightsLightFunctions(
	TEXT("r.MegaLights.LightFunctions"),
	0,
	TEXT("Whether to support light functions."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarMegaLightsIESProfiles(
	TEXT("r.MegaLights.IESProfiles"),
	1,
	TEXT("Whether to support IES profiles on lights."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

namespace MegaLights
{
	// must match values in MegaLights.ush
	constexpr int32 TileSize = 8;
	constexpr int32 MaxLocalLightIndexXY = 16; // 16 * 16 = 256

	bool IsEnabled()
	{
		return CVarMegaLights.GetValueOnRenderThread() != 0;
	}

	bool IsUsingLightFunctions()
	{
		return IsEnabled() && CVarMegaLightsLightFunctions.GetValueOnRenderThread() != 0;
	}

	bool IsLightSupported(uint8 LightType, ECastRayTracedShadow::Type CastRayTracedShadow)
	{
		if (MegaLights::IsEnabled() && LightType != LightType_Directional)
		{
			const bool bRayTracedShadows = (CastRayTracedShadow == ECastRayTracedShadow::Enabled || (ShouldRenderRayTracingShadows() && CastRayTracedShadow == ECastRayTracedShadow::UseProjectSetting));
			return CVarMegaLights.GetValueOnRenderThread() == 2 || bRayTracedShadows;
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
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM6) && RHISupportsWaveOperations(Parameters.Platform);
	}

	uint32 GetStateFrameIndex(FSceneViewState* ViewState)
	{
		uint32 StateFrameIndex = ViewState ? ViewState->GetFrameIndex() : 0;

		if (CVarMegaLightsFixedStateFrameIndex.GetValueOnRenderThread() >= 0)
		{
			StateFrameIndex = CVarMegaLightsFixedStateFrameIndex.GetValueOnRenderThread();
		}

		return StateFrameIndex;
	}

	FIntPoint GetNumSamplesPerPixel2d(int32 NumSamplesPerPixel1d)
	{
		if (NumSamplesPerPixel1d >= 16)
		{
			return FIntPoint(4, 4);
		}
		else if (NumSamplesPerPixel1d >= 4)
		{
			return FIntPoint(2, 2);
		}
		else
		{
			return FIntPoint(2, 1);
		}
	}

	FIntPoint GetNumSamplesPerPixel2d()
	{
		return GetNumSamplesPerPixel2d(CVarMegaLightsNumSamplesPerPixel.GetValueOnAnyThread());
	}

	int32 GetDebugMode()
	{
		return CVarMegaLightsDebug.GetValueOnRenderThread();
	}

	bool UseWaveOps(EShaderPlatform ShaderPlatform)
	{
		return CVarMegaLightsWaveOps.GetValueOnRenderThread() != 0
			&& GRHISupportsWaveOperations
			&& RHISupportsWaveOperations(ShaderPlatform);
	}

	void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FForwardLightingParameters::ModifyCompilationEnvironment(Platform, OutEnvironment);
		ShaderPrint::ModifyCompilationEnvironment(Platform, OutEnvironment);
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
		SHADER_PARAMETER_STRUCT_INCLUDE(FMegaLightsParameters, MegaLightsParameters)
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
		return MegaLights::ShouldCompileShaders(Parameters);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());
	}
};

IMPLEMENT_GLOBAL_SHADER(FTileClassificationCS, "/Engine/Private/MegaLights/MegaLights.usf", "TileClassificationCS", SF_Compute);

class FInitTileIndirectArgsCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FInitTileIndirectArgsCS)
	SHADER_USE_PARAMETER_STRUCT(FInitTileIndirectArgsCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FMegaLightsParameters, MegaLightsParameters)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWTileIndirectArgs)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWDownsampledTileIndirectArgs)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, TileAllocator)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, DownsampledTileAllocator)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return MegaLights::ShouldCompileShaders(Parameters);
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

IMPLEMENT_GLOBAL_SHADER(FInitTileIndirectArgsCS, "/Engine/Private/MegaLights/MegaLights.usf", "InitTileIndirectArgsCS", SF_Compute);

class FGenerateLightSamplesCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FGenerateLightSamplesCS)
	SHADER_USE_PARAMETER_STRUCT(FGenerateLightSamplesCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		RDG_BUFFER_ACCESS(IndirectArgs, ERHIAccess::IndirectArgs)
		SHADER_PARAMETER_STRUCT_INCLUDE(FMegaLightsParameters, MegaLightsParameters)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float>, RWDownsampledSceneDepth)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<UNORM float3>, RWDownsampledSceneWorldNormal)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<uint>, RWLightSamples)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, DownsampledTileAllocator)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, DownsampledTileData)
	END_SHADER_PARAMETER_STRUCT()

	class FTileType : SHADER_PERMUTATION_INT("TILE_TYPE", (int32)MegaLights::ETileType::SHADING_MAX);
	class FIESProfile : SHADER_PERMUTATION_BOOL("USE_IES_PROFILE");
	class FLightFunctionAtlas : SHADER_PERMUTATION_BOOL("USE_LIGHT_FUNCTION_ATLAS");
	class FTexturedRectLights : SHADER_PERMUTATION_BOOL("USE_SOURCE_TEXTURE");
	class FNumSamplesPerPixel1d : SHADER_PERMUTATION_SPARSE_INT("NUM_SAMPLES_PER_PIXEL_1D", 2, 4, 16);
	class FDebugMode : SHADER_PERMUTATION_BOOL("DEBUG_MODE");
	using FPermutationDomain = TShaderPermutationDomain<FTileType, FIESProfile, FLightFunctionAtlas, FTexturedRectLights, FNumSamplesPerPixel1d, FDebugMode>;

	static int32 GetGroupSize()
	{	
		return 8;
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return MegaLights::ShouldCompileShaders(Parameters);
	}

	static EShaderPermutationPrecacheRequest ShouldPrecachePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		FPermutationDomain PermutationVector(Parameters.PermutationId);

		// precache all tile types

		if (PermutationVector.Get<FIESProfile>() != (CVarMegaLightsIESProfiles.GetValueOnAnyThread() != 0))
		{
			return EShaderPermutationPrecacheRequest::NotUsed;
		}
		
		if (PermutationVector.Get<FTexturedRectLights>() != (CVarMegaLightsTexturedRectLights.GetValueOnAnyThread() != 0))
		{
			return EShaderPermutationPrecacheRequest::NotUsed;
		}

		int NumSamplesPerPixel1d = PermutationVector.Get<FNumSamplesPerPixel1d>();
		const FIntPoint NumSamplesPerPixel2d = MegaLights::GetNumSamplesPerPixel2d();
		if (NumSamplesPerPixel1d != (NumSamplesPerPixel2d.X * NumSamplesPerPixel2d.Y))
		{
			return EShaderPermutationPrecacheRequest::NotUsed;
		}
		
		if (PermutationVector.Get<FDebugMode>())
		{
			return EShaderPermutationPrecacheRequest::NotPrecached;
		}

		return EShaderPermutationPrecacheRequest::Precached;
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		MegaLights::ModifyCompilationEnvironment(Parameters.Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());
		OutEnvironment.CompilerFlags.Add(CFLAG_WaveOperations);

		FPermutationDomain PermutationVector(Parameters.PermutationId);
		const int32 NumSamplesPerPixel1d = PermutationVector.Get<FNumSamplesPerPixel1d>();
		const FIntPoint NumSamplesPerPixel2d = MegaLights::GetNumSamplesPerPixel2d(NumSamplesPerPixel1d);
		OutEnvironment.SetDefine(TEXT("NUM_SAMPLES_PER_PIXEL_2D_X"), NumSamplesPerPixel2d.X);
		OutEnvironment.SetDefine(TEXT("NUM_SAMPLES_PER_PIXEL_2D_Y"), NumSamplesPerPixel2d.Y);
	}
};

IMPLEMENT_GLOBAL_SHADER(FGenerateLightSamplesCS, "/Engine/Private/MegaLights/MegaLightsSampling.usf", "GenerateLightSamplesCS", SF_Compute);

class FClearLightSamplesCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FClearLightSamplesCS)
	SHADER_USE_PARAMETER_STRUCT(FClearLightSamplesCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		RDG_BUFFER_ACCESS(IndirectArgs, ERHIAccess::IndirectArgs)
		SHADER_PARAMETER_STRUCT_INCLUDE(FMegaLightsParameters, MegaLightsParameters)
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
		return MegaLights::ShouldCompileShaders(Parameters);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		MegaLights::ModifyCompilationEnvironment(Parameters.Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());
	}
};

IMPLEMENT_GLOBAL_SHADER(FClearLightSamplesCS, "/Engine/Private/MegaLights/MegaLightsSampling.usf", "ClearLightSamplesCS", SF_Compute);

class FInitCompositeUpsampleWeightsCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FInitCompositeUpsampleWeightsCS)
	SHADER_USE_PARAMETER_STRUCT(FInitCompositeUpsampleWeightsCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FMegaLightsParameters, MegaLightsParameters)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, RWCompositeUpsampleWeights)
	END_SHADER_PARAMETER_STRUCT()

	static int32 GetGroupSize()
	{
		return 8;
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return MegaLights::ShouldCompileShaders(Parameters);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());
	}
};

IMPLEMENT_GLOBAL_SHADER(FInitCompositeUpsampleWeightsCS, "/Engine/Private/MegaLights/MegaLights.usf", "InitCompositeUpsampleWeightsCS", SF_Compute);

class FShadeLightSamplesCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FShadeLightSamplesCS)
	SHADER_USE_PARAMETER_STRUCT(FShadeLightSamplesCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		RDG_BUFFER_ACCESS(IndirectArgs, ERHIAccess::IndirectArgs)
		SHADER_PARAMETER_STRUCT_INCLUDE(FMegaLightsParameters, MegaLightsParameters)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float3>, RWResolvedDiffuseLighting)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float3>, RWResolvedSpecularLighting)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, TileAllocator)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, TileData)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<flaot4>, CompositeUpsampleWeights)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<uint>, LightSamples)
	END_SHADER_PARAMETER_STRUCT()

	static int32 GetGroupSize()
	{
		return 8;
	}

	class FTileType : SHADER_PERMUTATION_INT("TILE_TYPE", (int32)MegaLights::ETileType::SHADING_MAX);
	class FIESProfile : SHADER_PERMUTATION_BOOL("USE_IES_PROFILE");
	class FLightFunctionAtlas : SHADER_PERMUTATION_BOOL("USE_LIGHT_FUNCTION_ATLAS");
	class FTexturedRectLights : SHADER_PERMUTATION_BOOL("USE_SOURCE_TEXTURE");
	class FNumSamplesPerPixel1d : SHADER_PERMUTATION_SPARSE_INT("NUM_SAMPLES_PER_PIXEL_1D", 2, 4, 16);
	class FDebugMode : SHADER_PERMUTATION_BOOL("DEBUG_MODE");
	using FPermutationDomain = TShaderPermutationDomain<FTileType, FIESProfile, FLightFunctionAtlas, FTexturedRectLights, FNumSamplesPerPixel1d, FDebugMode>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return MegaLights::ShouldCompileShaders(Parameters);
	}

	static EShaderPermutationPrecacheRequest ShouldPrecachePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		FPermutationDomain PermutationVector(Parameters.PermutationId);

		if (PermutationVector.Get<FIESProfile>() != (CVarMegaLightsIESProfiles.GetValueOnAnyThread() != 0))
		{
			return EShaderPermutationPrecacheRequest::NotUsed;
		}

		if (PermutationVector.Get<FTexturedRectLights>() != (CVarMegaLightsTexturedRectLights.GetValueOnAnyThread() != 0))
		{
			return EShaderPermutationPrecacheRequest::NotUsed;
		}

		if (PermutationVector.Get<FDebugMode>())
		{
			return EShaderPermutationPrecacheRequest::NotPrecached;
		}

		return EShaderPermutationPrecacheRequest::Precached;
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		MegaLights::ModifyCompilationEnvironment(Parameters.Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());

		FPermutationDomain PermutationVector(Parameters.PermutationId);
		const int32 NumSamplesPerPixel1d = PermutationVector.Get<FNumSamplesPerPixel1d>();
		const FIntPoint NumSamplesPerPixel2d = MegaLights::GetNumSamplesPerPixel2d(NumSamplesPerPixel1d);
		OutEnvironment.SetDefine(TEXT("NUM_SAMPLES_PER_PIXEL_2D_X"), NumSamplesPerPixel2d.X);
		OutEnvironment.SetDefine(TEXT("NUM_SAMPLES_PER_PIXEL_2D_Y"), NumSamplesPerPixel2d.Y);
	}
};

IMPLEMENT_GLOBAL_SHADER(FShadeLightSamplesCS, "/Engine/Private/MegaLights/MegaLightsShading.usf", "ShadeLightSamplesCS", SF_Compute);

class FClearResolvedLightingCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FClearResolvedLightingCS)
	SHADER_USE_PARAMETER_STRUCT(FClearResolvedLightingCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		RDG_BUFFER_ACCESS(IndirectArgs, ERHIAccess::IndirectArgs)
		SHADER_PARAMETER_STRUCT_INCLUDE(FMegaLightsParameters, MegaLightsParameters)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float3>, RWResolvedDiffuseLighting)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float3>, RWResolvedSpecularLighting)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, TileAllocator)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, TileData)
	END_SHADER_PARAMETER_STRUCT()

	static int32 GetGroupSize()
	{
		return 8;
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return MegaLights::ShouldCompileShaders(Parameters);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		MegaLights::ModifyCompilationEnvironment(Parameters.Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());
	}
};

IMPLEMENT_GLOBAL_SHADER(FClearResolvedLightingCS, "/Engine/Private/MegaLights/MegaLightsShading.usf", "ClearResolvedLightingCS", SF_Compute);

class FDenoiserTemporalCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FDenoiserTemporalCS)
	SHADER_USE_PARAMETER_STRUCT(FDenoiserTemporalCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FMegaLightsParameters, MegaLightsParameters)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float4>, ResolvedDiffuseLighting)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float4>, ResolvedSpecularLighting)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float4>, DiffuseLightingAndSecondMomentHistoryTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float4>, SpecularLightingAndSecondMomentHistoryTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<UNORM float>, NumFramesAccumulatedHistoryTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float>, MegaLightsDepthHistory)
		SHADER_PARAMETER(FVector4f, HistoryScreenPositionScaleBias)
		SHADER_PARAMETER(FVector4f, HistoryUVMinMax)
		SHADER_PARAMETER(FVector4f, HistoryGatherUVMinMax)
		SHADER_PARAMETER(float, PrevSceneColorPreExposureCorrection)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, RWDiffuseLightingAndSecondMoment)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, RWSpecularLightingAndSecondMoment)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<UNORM float>, RWNumFramesAccumulated)
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
		return MegaLights::ShouldCompileShaders(Parameters);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());
	}
};

IMPLEMENT_GLOBAL_SHADER(FDenoiserTemporalCS, "/Engine/Private/MegaLights/MegaLightsDenoiserTemporal.usf", "DenoiserTemporalCS", SF_Compute);

class FDenoiserSpatialCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FDenoiserSpatialCS)
	SHADER_USE_PARAMETER_STRUCT(FDenoiserSpatialCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FMegaLightsParameters, MegaLightsParameters)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, RWSceneColor)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float3>, DiffuseLightingAndSecondMomentTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float3>, SpecularLightingAndSecondMomentTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<UNORM float>, NumFramesAccumulatedTexture)
		SHADER_PARAMETER(float, SpatialFilterDepthWeightScale)
		SHADER_PARAMETER(float, SpatialFilterKernelRadius)
		SHADER_PARAMETER(uint32, SpatialFilterNumSamples)
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
		return MegaLights::ShouldCompileShaders(Parameters);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());
	}
};

IMPLEMENT_GLOBAL_SHADER(FDenoiserSpatialCS, "/Engine/Private/MegaLights/MegaLightsDenoiserSpatial.usf", "DenoiserSpatialCS", SF_Compute);

DECLARE_GPU_STAT(MegaLights);

/**
 * Single pass batched light rendering using ray tracing (distance field or triangle) for stochastic light (BRDF and visibility) sampling.
 */
void FDeferredShadingSceneRenderer::RenderMegaLights(FRDGBuilder& GraphBuilder, const FSceneTextures& SceneTextures)
{
	if (!MegaLights::IsEnabled())
	{
		return;
	}

	check(AreLightsInLightGrid());
	RDG_EVENT_SCOPE(GraphBuilder, "MegaLights");
	RDG_GPU_STAT_SCOPE(GraphBuilder, MegaLights);

	FBlueNoise BlueNoise = GetBlueNoiseGlobalParameters();
	TUniformBufferRef<FBlueNoise> BlueNoiseUniformBuffer = CreateUniformBufferImmediate(BlueNoise, EUniformBufferUsage::UniformBuffer_SingleDraw);

	for (int32 ViewIndex = 0; ViewIndex < AllViews.Num(); ++ViewIndex)
	{
		const FViewInfo& View = Views[ViewIndex];

		// History reset for debugging purposes
		bool bResetHistory = false;

		if (GMegaLightsResetEveryNthFrame > 0 && (ViewFamily.FrameNumber % (uint32)GMegaLightsResetEveryNthFrame) == 0)
		{
			bResetHistory = true;
		}

		if (GMegaLightsReset != 0)
		{
			GMegaLightsReset = 0;
			bResetHistory = true;
		}

		const bool bDebug = MegaLights::GetDebugMode() != 0;
		const bool bWaveOps = MegaLights::UseWaveOps(View.GetShaderPlatform())
			&& GRHIMinimumWaveSize <= 32
			&& GRHIMaximumWaveSize >= 32;

		const FIntPoint NumSamplesPerPixel2d = MegaLights::GetNumSamplesPerPixel2d();

		const uint32 DownsampleFactor = 2;
		const FIntPoint DownsampledViewSize = FIntPoint::DivideAndRoundUp(View.ViewRect.Size(), DownsampleFactor);
		const FIntPoint SampleViewSize = DownsampledViewSize * NumSamplesPerPixel2d;
		const FIntPoint DownsampledBufferSize = FIntPoint::DivideAndRoundUp(SceneTextures.Config.Extent, DownsampleFactor);
		const FIntPoint SampleBufferSize = DownsampledBufferSize * NumSamplesPerPixel2d;
		const FIntPoint DonwnsampledSampleBufferSize = DownsampledBufferSize * NumSamplesPerPixel2d;

		FRDGTextureRef DownsampledSceneDepth = GraphBuilder.CreateTexture(
			FRDGTextureDesc::Create2D(DownsampledBufferSize, PF_R32_FLOAT, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV),
			TEXT("MegaLights.DownsampledSceneDepth"));

		FRDGTextureRef DownsampledSceneWorldNormal = GraphBuilder.CreateTexture(
			FRDGTextureDesc::Create2D(DownsampledBufferSize, PF_A2B10G10R10, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV),
			TEXT("MegaLights.DownsampledSceneWorldNormal"));

		FRDGTextureRef LightSamples = GraphBuilder.CreateTexture(
			FRDGTextureDesc::Create2D(DonwnsampledSampleBufferSize, PF_R32_UINT, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV),
			TEXT("MegaLights.LightSamples"));

		FRDGTextureRef LightSampleRayDistance = GraphBuilder.CreateTexture(
			FRDGTextureDesc::Create2D(DonwnsampledSampleBufferSize, PF_R16F, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV),
			TEXT("MegaLights.LightSampleRayDistance"));

		bool bTemporal = CVarMegaLightsTemporal.GetValueOnRenderThread() != 0;
		FVector4f HistoryScreenPositionScaleBias = FVector4f(0.0f, 0.0f, 0.0f, 0.0f);
		FVector4f HistoryUVMinMax = FVector4f(0.0f, 0.0f, 0.0f, 0.0f);
		FVector4f HistoryGatherUVMinMax = FVector4f(0.0f, 0.0f, 0.0f, 0.0f);
		FRDGTextureRef DiffuseLightingAndSecondMomentHistory = nullptr;
		FRDGTextureRef SpecularLightingAndSecondMomentHistory = nullptr;
		FRDGTextureRef SceneDepthHistory = nullptr;
		FRDGTextureRef NumFramesAccumulatedHistory = nullptr;

		if (View.ViewState)
		{
			const FMegaLightsViewState& MegaLightsViewState = View.ViewState->MegaLights;
			const FStochasticLightingViewState& StochasticLightingViewState = View.ViewState->StochasticLighting;

			if (!View.bCameraCut 
				&& !View.bPrevTransformsReset
				&& !bResetHistory 
				&& bTemporal)
			{
				HistoryScreenPositionScaleBias = MegaLightsViewState.HistoryScreenPositionScaleBias;
				HistoryUVMinMax = MegaLightsViewState.HistoryUVMinMax;
				HistoryGatherUVMinMax = MegaLightsViewState.HistoryGatherUVMinMax;

				if (MegaLightsViewState.DiffuseLightingAndSecondMomentHistory
					&& MegaLightsViewState.SpecularLightingAndSecondMomentHistory
					&& StochasticLightingViewState.SceneDepthHistory
					&& MegaLightsViewState.NumFramesAccumulatedHistory
					&& MegaLightsViewState.DiffuseLightingAndSecondMomentHistory->GetDesc().Extent == View.GetSceneTexturesConfig().Extent
					&& MegaLightsViewState.SpecularLightingAndSecondMomentHistory->GetDesc().Extent == View.GetSceneTexturesConfig().Extent
					&& StochasticLightingViewState.SceneDepthHistory->GetDesc().Extent == SceneTextures.Depth.Resolve->Desc.Extent)
				{
					DiffuseLightingAndSecondMomentHistory = GraphBuilder.RegisterExternalTexture(MegaLightsViewState.DiffuseLightingAndSecondMomentHistory);
					SpecularLightingAndSecondMomentHistory = GraphBuilder.RegisterExternalTexture(MegaLightsViewState.SpecularLightingAndSecondMomentHistory);
					NumFramesAccumulatedHistory = GraphBuilder.RegisterExternalTexture(MegaLightsViewState.NumFramesAccumulatedHistory);
					SceneDepthHistory = GraphBuilder.RegisterExternalTexture(StochasticLightingViewState.SceneDepthHistory);
				}
			}
		}

		// Setup the light function atlas
		const bool bUseLightFunctionAtlas = LightFunctionAtlas::IsEnabled(View, LightFunctionAtlas::ELightFunctionAtlasSystem::MegaLights);

		const FIntPoint ViewSizeInTiles = FIntPoint::DivideAndRoundUp(View.ViewRect.Size(), MegaLights::TileSize);
		const int32 TileDataStride = ViewSizeInTiles.X * ViewSizeInTiles.Y;

		const FIntPoint DownsampledViewSizeInTiles = FIntPoint::DivideAndRoundUp(DownsampledViewSize, MegaLights::TileSize);
		const int32 DownsampledTileDataStride = DownsampledViewSizeInTiles.X * DownsampledViewSizeInTiles.Y;

		FRDGTextureRef DownsampledTileMask = GraphBuilder.CreateTexture(
			FRDGTextureDesc::Create2D(FMath::DivideAndRoundUp<FIntPoint>(DownsampledBufferSize, MegaLights::TileSize), PF_R8_UINT, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV),
			TEXT("MegaLights.DownsampledTileMask"));

		FMegaLightsParameters MegaLightsParameters;
		{
			MegaLightsParameters.ViewUniformBuffer = View.ViewUniformBuffer;
			MegaLightsParameters.Scene = View.GetSceneUniforms().GetBuffer(GraphBuilder);
			MegaLightsParameters.SceneTextures = GetSceneTextureParameters(GraphBuilder, SceneTextures.UniformBuffer);
			MegaLightsParameters.SceneTexturesStruct = SceneTextures.UniformBuffer;
			MegaLightsParameters.Substrate = Substrate::BindSubstrateGlobalUniformParameters(View);
			MegaLightsParameters.ForwardLightData = View.ForwardLightingResources.ForwardLightUniformBuffer;
			MegaLightsParameters.LightFunctionAtlas = LightFunctionAtlas::BindGlobalParameters(GraphBuilder, View);
			MegaLightsParameters.BlueNoise = BlueNoiseUniformBuffer;
			MegaLightsParameters.PreIntegratedGF = GSystemTextures.PreintegratedGF->GetRHI();
			MegaLightsParameters.PreIntegratedGFSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
			MegaLightsParameters.DownsampledViewMin = FIntPoint::DivideAndRoundUp(View.ViewRect.Min, DownsampleFactor);
			MegaLightsParameters.DownsampledViewSize = DownsampledViewSize;
			MegaLightsParameters.SampleViewMin = FIntPoint::DivideAndRoundUp(View.ViewRect.Min, DownsampleFactor) * NumSamplesPerPixel2d;
			MegaLightsParameters.SampleViewSize = SampleViewSize;
			MegaLightsParameters.NumSamplesPerPixel = NumSamplesPerPixel2d;
			MegaLightsParameters.NumSamplesPerPixelDivideShift.X = FMath::FloorLog2(NumSamplesPerPixel2d.X);
			MegaLightsParameters.NumSamplesPerPixelDivideShift.Y = FMath::FloorLog2(NumSamplesPerPixel2d.Y);
			MegaLightsParameters.MegaLightsStateFrameIndex = MegaLights::GetStateFrameIndex(View.ViewState);
			MegaLightsParameters.DownsampledTileMask = DownsampledTileMask;
			MegaLightsParameters.DownsampledSceneDepth = DownsampledSceneDepth;
			MegaLightsParameters.DownsampledSceneWorldNormal = DownsampledSceneWorldNormal;
			MegaLightsParameters.DownsampledBufferInvSize = FVector2f(1.0f) / DownsampledBufferSize;
			MegaLightsParameters.SamplingMinWeight = FMath::Max(CVarMegaLightsSamplingMinWeight.GetValueOnRenderThread(), 0.0f);
			MegaLightsParameters.TileDataStride = TileDataStride;
			MegaLightsParameters.DownsampledTileDataStride = DownsampledTileDataStride;
			MegaLightsParameters.TemporalMaxFramesAccumulated = FMath::Max(CVarMegaLightsTemporalMaxFramesAccumulated.GetValueOnRenderThread(), 0.0f);
			MegaLightsParameters.TemporalNeighborhoodClampScale = CVarMegaLightsTemporalNeighborhoodClampScale.GetValueOnRenderThread();
			MegaLightsParameters.TemporalAdvanceFrame = View.ViewState && !View.bStatePrevViewInfoIsReadOnly ? 1 : 0;
			MegaLightsParameters.DebugMode = MegaLights::GetDebugMode();
			MegaLightsParameters.DebugLightId = INDEX_NONE;

			if (bDebug)
			{
				ShaderPrint::SetEnabled(true);
				ShaderPrint::RequestSpaceForLines(1024);
				ShaderPrint::SetParameters(GraphBuilder, View.ShaderPrintData, MegaLightsParameters.ShaderPrintUniformBuffer);

				MegaLightsParameters.DebugLightId = CVarMegaLightsDebugLightId.GetValueOnRenderThread();

				if (MegaLightsParameters.DebugLightId < 0)
				{
					for (auto LightIt = Scene->Lights.CreateConstIterator(); LightIt; ++LightIt)
					{
						const FLightSceneInfoCompact& LightSceneInfoCompact = *LightIt;
						const FLightSceneInfo* const LightSceneInfo = LightSceneInfoCompact.LightSceneInfo;

						if (LightSceneInfo->Proxy->IsSelected())
						{
							MegaLightsParameters.DebugLightId = LightSceneInfo->Id;
							break;
						}
					}
				}
			}
		}

		FRDGBufferRef TileAllocator = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), (int32)MegaLights::ETileType::MAX), TEXT("MegaLights.TileAllocator"));
		FRDGBufferRef TileData = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), TileDataStride * (int32)MegaLights::ETileType::MAX), TEXT("MegaLights.TileData"));
		AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(TileAllocator), 0);

		FRDGBufferRef DownsampledTileAllocator = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), (int32)MegaLights::ETileType::MAX), TEXT("MegaLights.DownsampledTileAllocator"));
		FRDGBufferRef DownsampledTileData = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), DownsampledTileDataStride * (int32)MegaLights::ETileType::MAX), TEXT("MegaLights.DownsampledTileData"));
		AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(DownsampledTileAllocator), 0);

		// #ml_todo: merge classification passes or reuse downsampled one to create full res tiles
		// Run tile classification to generate tiles for the subsequent passes
		{
			{
				FTileClassificationCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FTileClassificationCS::FParameters>();
				PassParameters->MegaLightsParameters = MegaLightsParameters;
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
				PassParameters->MegaLightsParameters = MegaLightsParameters;
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

		FRDGBufferRef TileIndirectArgs = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateIndirectDesc<FRHIDispatchIndirectParameters>((int32)MegaLights::ETileType::MAX), TEXT("MegaLights.TileIndirectArgs"));
		FRDGBufferRef DownsampledTileIndirectArgs = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateIndirectDesc<FRHIDispatchIndirectParameters>((int32)MegaLights::ETileType::MAX), TEXT("MegaLights.DownsampledTileIndirectArgs"));

		// Setup indirect args for classified tiles
		{
			FInitTileIndirectArgsCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FInitTileIndirectArgsCS::FParameters>();
			PassParameters->MegaLightsParameters = MegaLightsParameters;
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

		// Generate new candidate light samples
		{
			FRDGTextureUAVRef DownsampledSceneDepthUAV = GraphBuilder.CreateUAV(DownsampledSceneDepth, ERDGUnorderedAccessViewFlags::SkipBarrier);
			FRDGTextureUAVRef DownsampledSceneWorldNormalUAV = GraphBuilder.CreateUAV(DownsampledSceneWorldNormal, ERDGUnorderedAccessViewFlags::SkipBarrier);
			FRDGTextureUAVRef LightSamplesUAV = GraphBuilder.CreateUAV(LightSamples, ERDGUnorderedAccessViewFlags::SkipBarrier);

			// Clear tiles which don't contain any lights or geometry
			{
				FClearLightSamplesCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FClearLightSamplesCS::FParameters>();
				PassParameters->IndirectArgs = DownsampledTileIndirectArgs;
				PassParameters->MegaLightsParameters = MegaLightsParameters;
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
					(int32)MegaLights::ETileType::Empty * sizeof(FRHIDispatchIndirectParameters));
			}

			for (int32 TileType = 0; TileType < (int32)MegaLights::ETileType::SHADING_MAX; ++TileType)
			{
				FGenerateLightSamplesCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FGenerateLightSamplesCS::FParameters>();
				PassParameters->IndirectArgs = DownsampledTileIndirectArgs;
				PassParameters->MegaLightsParameters = MegaLightsParameters;
				PassParameters->RWDownsampledSceneDepth = DownsampledSceneDepthUAV;
				PassParameters->RWDownsampledSceneWorldNormal = DownsampledSceneWorldNormalUAV;
				PassParameters->RWLightSamples = LightSamplesUAV;
				PassParameters->DownsampledTileAllocator = GraphBuilder.CreateSRV(DownsampledTileAllocator);
				PassParameters->DownsampledTileData = GraphBuilder.CreateSRV(DownsampledTileData);

				FGenerateLightSamplesCS::FPermutationDomain PermutationVector;
				PermutationVector.Set<FGenerateLightSamplesCS::FTileType>(TileType);
				PermutationVector.Set<FGenerateLightSamplesCS::FIESProfile>(CVarMegaLightsIESProfiles.GetValueOnRenderThread() != 0);
				PermutationVector.Set<FGenerateLightSamplesCS::FLightFunctionAtlas>(bUseLightFunctionAtlas);
				PermutationVector.Set<FGenerateLightSamplesCS::FTexturedRectLights>(CVarMegaLightsTexturedRectLights.GetValueOnRenderThread() != 0);
				PermutationVector.Set<FGenerateLightSamplesCS::FNumSamplesPerPixel1d>(NumSamplesPerPixel2d.X * NumSamplesPerPixel2d.Y);
				PermutationVector.Set<FGenerateLightSamplesCS::FDebugMode>(bDebug);
				auto ComputeShader = View.ShaderMap->GetShader<FGenerateLightSamplesCS>(PermutationVector);

				FComputeShaderUtils::AddPass(
					GraphBuilder,
					RDG_EVENT_NAME("GenerateSamples SamplesPerPixel:%dx%d TileType:%d", NumSamplesPerPixel2d.X, NumSamplesPerPixel2d.Y, TileType),
					ComputeShader,
					PassParameters,
					DownsampledTileIndirectArgs,
					TileType * sizeof(FRHIDispatchIndirectParameters));
			}
		}

		MegaLights::RayTraceLightSamples(
			ViewFamily,
			View,
			GraphBuilder,
			SceneTextures,
			SampleBufferSize,
			LightSamples,
			LightSampleRayDistance,
			MegaLightsParameters
		);

		FRDGTextureRef CompositeUpsampleWeights = GraphBuilder.CreateTexture(
			FRDGTextureDesc::Create2D(SceneTextures.Config.Extent, PF_FloatRGBA, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV),
			TEXT("MegaLights.CompositeUpsampleWeights"));

		// Init composite upsample weights
		{
			FInitCompositeUpsampleWeightsCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FInitCompositeUpsampleWeightsCS::FParameters>();
			PassParameters->MegaLightsParameters = MegaLightsParameters;
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

		FRDGTextureRef ResolvedDiffuseLighting = GraphBuilder.CreateTexture(
			FRDGTextureDesc::Create2D(View.GetSceneTexturesConfig().Extent, PF_FloatRGB, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV),
			TEXT("MegaLights.ResolvedDiffuseLighting"));

		FRDGTextureRef ResolvedSpecularLighting = GraphBuilder.CreateTexture(
			FRDGTextureDesc::Create2D(View.GetSceneTexturesConfig().Extent, PF_FloatRGB, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV),
			TEXT("MegaLights.ResolvedSpecularLighting"));

		// Shade light samples
		{
			FRDGTextureUAVRef ResolvedDiffuseLightingUAV = GraphBuilder.CreateUAV(ResolvedDiffuseLighting, ERDGUnorderedAccessViewFlags::SkipBarrier);
			FRDGTextureUAVRef ResolvedSpecularLightingUAV = GraphBuilder.CreateUAV(ResolvedSpecularLighting, ERDGUnorderedAccessViewFlags::SkipBarrier);

			// Clear tiles which won't be processed by FShadeLightSamplesCS
			{
				FClearResolvedLightingCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FClearResolvedLightingCS::FParameters>();
				PassParameters->IndirectArgs = TileIndirectArgs;
				PassParameters->RWResolvedDiffuseLighting = ResolvedDiffuseLightingUAV;
				PassParameters->RWResolvedSpecularLighting = ResolvedSpecularLightingUAV;
				PassParameters->MegaLightsParameters = MegaLightsParameters;
				PassParameters->TileAllocator = GraphBuilder.CreateSRV(TileAllocator);
				PassParameters->TileData = GraphBuilder.CreateSRV(TileData);

				auto ComputeShader = View.ShaderMap->GetShader<FClearResolvedLightingCS>();

				FComputeShaderUtils::AddPass(
					GraphBuilder,
					RDG_EVENT_NAME("ClearResolvedLighting"),
					ComputeShader,
					PassParameters,
					TileIndirectArgs,
					(int32)MegaLights::ETileType::Empty * sizeof(FRHIDispatchIndirectParameters));
			}

			for (int32 TileType = 0; TileType < (int32)MegaLights::ETileType::SHADING_MAX; ++TileType)
			{
				FShadeLightSamplesCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FShadeLightSamplesCS::FParameters>();
				PassParameters->RWResolvedDiffuseLighting = ResolvedDiffuseLightingUAV;
				PassParameters->RWResolvedSpecularLighting = ResolvedSpecularLightingUAV;
				PassParameters->IndirectArgs = TileIndirectArgs;
				PassParameters->MegaLightsParameters = MegaLightsParameters;
				PassParameters->TileAllocator = GraphBuilder.CreateSRV(TileAllocator);
				PassParameters->TileData = GraphBuilder.CreateSRV(TileData);
				PassParameters->CompositeUpsampleWeights = CompositeUpsampleWeights;
				PassParameters->LightSamples = LightSamples;

				FShadeLightSamplesCS::FPermutationDomain PermutationVector;
				PermutationVector.Set<FShadeLightSamplesCS::FTileType>(TileType);
				PermutationVector.Set<FShadeLightSamplesCS::FIESProfile>(CVarMegaLightsIESProfiles.GetValueOnRenderThread() != 0);
				PermutationVector.Set<FShadeLightSamplesCS::FLightFunctionAtlas>(bUseLightFunctionAtlas);
				PermutationVector.Set<FShadeLightSamplesCS::FTexturedRectLights>(CVarMegaLightsTexturedRectLights.GetValueOnRenderThread() != 0);
				PermutationVector.Set<FShadeLightSamplesCS::FNumSamplesPerPixel1d>(NumSamplesPerPixel2d.X * NumSamplesPerPixel2d.Y);
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
			TEXT("MegaLights.DiffuseLightingAndSecondMoment"));

		FRDGTextureRef SpecularLightingAndSecondMoment = GraphBuilder.CreateTexture(
			FRDGTextureDesc::Create2D(View.GetSceneTexturesConfig().Extent, PF_FloatRGBA, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV),
			TEXT("MegaLights.SpecularLightingAndSecondMoment"));

		FRDGTextureRef NumFramesAccumulated = GraphBuilder.CreateTexture(
			FRDGTextureDesc::Create2D(View.GetSceneTexturesConfig().Extent, PF_G8, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV),
			TEXT("MegaLights.NumFramesAccumulated"));

		// Temporal accumulation
		{
			FDenoiserTemporalCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FDenoiserTemporalCS::FParameters>();
			PassParameters->MegaLightsParameters = MegaLightsParameters;
			PassParameters->ResolvedDiffuseLighting = ResolvedDiffuseLighting;
			PassParameters->ResolvedSpecularLighting = ResolvedSpecularLighting;
			PassParameters->DiffuseLightingAndSecondMomentHistoryTexture = DiffuseLightingAndSecondMomentHistory;
			PassParameters->SpecularLightingAndSecondMomentHistoryTexture = SpecularLightingAndSecondMomentHistory;
			PassParameters->NumFramesAccumulatedHistoryTexture = NumFramesAccumulatedHistory;
			PassParameters->MegaLightsDepthHistory = SceneDepthHistory;
			PassParameters->PrevSceneColorPreExposureCorrection = View.PreExposure / View.PrevViewInfo.SceneColorPreExposure;
			PassParameters->HistoryScreenPositionScaleBias = HistoryScreenPositionScaleBias;
			PassParameters->HistoryUVMinMax = HistoryUVMinMax;
			PassParameters->HistoryGatherUVMinMax = HistoryGatherUVMinMax;
			PassParameters->RWDiffuseLightingAndSecondMoment = GraphBuilder.CreateUAV(DiffuseLightingAndSecondMoment);
			PassParameters->RWSpecularLightingAndSecondMoment = GraphBuilder.CreateUAV(SpecularLightingAndSecondMoment);
			PassParameters->RWNumFramesAccumulated = GraphBuilder.CreateUAV(NumFramesAccumulated);

			FDenoiserTemporalCS::FPermutationDomain PermutationVector;
			PermutationVector.Set<FDenoiserTemporalCS::FValidHistory>(DiffuseLightingAndSecondMomentHistory != nullptr && SceneDepthHistory && bTemporal);
			PermutationVector.Set<FDenoiserTemporalCS::FDebugMode>(bDebug);
			auto ComputeShader = View.ShaderMap->GetShader<FDenoiserTemporalCS>(PermutationVector);

			const FIntVector GroupCount = FComputeShaderUtils::GetGroupCount(View.ViewRect.Size(), FDenoiserTemporalCS::GetGroupSize());

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("TemporalAccumulation"),
				ComputeShader,
				PassParameters,
				GroupCount);
		}

		// Spatial filter
		{
			FDenoiserSpatialCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FDenoiserSpatialCS::FParameters>();
			PassParameters->MegaLightsParameters = MegaLightsParameters;
			PassParameters->RWSceneColor = GraphBuilder.CreateUAV(SceneTextures.Color.Target);
			PassParameters->DiffuseLightingAndSecondMomentTexture = DiffuseLightingAndSecondMoment;
			PassParameters->SpecularLightingAndSecondMomentTexture = SpecularLightingAndSecondMoment;
			PassParameters->NumFramesAccumulatedTexture = NumFramesAccumulated;
			PassParameters->SpatialFilterDepthWeightScale = CVarMegaLightsSpatialDepthWeightScale.GetValueOnRenderThread();
			PassParameters->SpatialFilterKernelRadius = CVarMegaLightsSpatialKernelRadius.GetValueOnRenderThread();
			PassParameters->SpatialFilterNumSamples = FMath::Clamp(CVarMegaLightsSpatialNumSamples.GetValueOnRenderThread(), 0, 1024);

			FDenoiserSpatialCS::FPermutationDomain PermutationVector;
			PermutationVector.Set<FDenoiserSpatialCS::FSpatialFilter>(CVarMegaLightsSpatial.GetValueOnRenderThread() != 0);
			PermutationVector.Set<FDenoiserSpatialCS::FDebugMode>(bDebug);
			auto ComputeShader = View.ShaderMap->GetShader<FDenoiserSpatialCS>(PermutationVector);

			const FIntVector GroupCount = FComputeShaderUtils::GetGroupCount(View.ViewRect.Size(), FDenoiserSpatialCS::GetGroupSize());

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("Spatial"),
				ComputeShader,
				PassParameters,
				GroupCount);
		}

		if (View.ViewState && !View.bStatePrevViewInfoIsReadOnly)
		{
			FMegaLightsViewState& MegaLightsViewState = View.ViewState->MegaLights;

			MegaLightsViewState.HistoryScreenPositionScaleBias = View.GetScreenPositionScaleBias(View.GetSceneTexturesConfig().Extent, View.ViewRect);

			const FVector2f InvBufferSize(1.0f / SceneTextures.Config.Extent.X, 1.0f / SceneTextures.Config.Extent.Y);

			MegaLightsViewState.HistoryUVMinMax = FVector4f(
				View.ViewRect.Min.X * InvBufferSize.X,
				View.ViewRect.Min.Y * InvBufferSize.Y,
				View.ViewRect.Max.X * InvBufferSize.X,
				View.ViewRect.Max.Y * InvBufferSize.Y);

			// Clamp gather4 to a valid bilinear footprint in order to avoid sampling outside of valid bounds
			MegaLightsViewState.HistoryGatherUVMinMax = FVector4f(
				(View.ViewRect.Min.X + 0.51f) * InvBufferSize.X,
				(View.ViewRect.Min.Y + 0.51f) * InvBufferSize.Y,
				(View.ViewRect.Max.X - 0.51f) * InvBufferSize.X,
				(View.ViewRect.Max.Y - 0.51f) * InvBufferSize.Y);

			if (DiffuseLightingAndSecondMoment && SpecularLightingAndSecondMoment && NumFramesAccumulated && bTemporal)
			{
				GraphBuilder.QueueTextureExtraction(DiffuseLightingAndSecondMoment, &MegaLightsViewState.DiffuseLightingAndSecondMomentHistory);
				GraphBuilder.QueueTextureExtraction(SpecularLightingAndSecondMoment, &MegaLightsViewState.SpecularLightingAndSecondMomentHistory);
				GraphBuilder.QueueTextureExtraction(NumFramesAccumulated, &MegaLightsViewState.NumFramesAccumulatedHistory);
			}
			else
			{
				MegaLightsViewState.DiffuseLightingAndSecondMomentHistory = nullptr;
				MegaLightsViewState.SpecularLightingAndSecondMomentHistory = nullptr;
				MegaLightsViewState.NumFramesAccumulatedHistory = nullptr;
			}
		}
	}
}