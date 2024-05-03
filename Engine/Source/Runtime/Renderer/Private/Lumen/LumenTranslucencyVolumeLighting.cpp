// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	LumenTranslucencyVolumeLighting.cpp
=============================================================================*/

#include "LumenTranslucencyVolumeLighting.h"
#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "SceneUtils.h"
#include "PipelineStateCache.h"
#include "ShaderParameterStruct.h"
#include "VolumeLighting.h"
#include "DistanceFieldLightingShared.h"
#include "LumenMeshCards.h"
#include "Math/Halton.h"
#include "DistanceFieldAmbientOcclusion.h"
#include "LumenTracingUtils.h"
#include "LumenRadianceCache.h"

#if RHI_RAYTRACING

#include "RayTracing/RaytracingOptions.h"
#include "RayTracing/RayTracingLighting.h"
#include "LumenHardwareRayTracingCommon.h"

#endif

TAutoConsoleVariable<int32> CVarLumenTranslucencyVolume(
	TEXT("r.Lumen.TranslucencyVolume.Enable"),
	1,
	TEXT(""),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarLumenTranslucencyVolumeTraceFromVolume(
	TEXT("r.Lumen.TranslucencyVolume.TraceFromVolume"),
	1,
	TEXT("Whether to ray trace from the translucency volume's voxels to gather indirect lighting.  Only makes sense to disable if TranslucencyVolume.RadianceCache is enabled."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarTranslucencyFroxelGridPixelSize(
	TEXT("r.Lumen.TranslucencyVolume.GridPixelSize"),
	32,
	TEXT("Size of a cell in the translucency grid, in pixels."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> CVarTranslucencyGridDistributionLogZScale(
	TEXT("r.Lumen.TranslucencyVolume.GridDistributionLogZScale"),
	.01f,
	TEXT(""),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> CVarTranslucencyGridDistributionLogZOffset(
	TEXT("r.Lumen.TranslucencyVolume.GridDistributionLogZOffset"),
	1.0f,
	TEXT(""),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> CVarTranslucencyGridDistributionZScale(
	TEXT("r.Lumen.TranslucencyVolume.GridDistributionZScale"),
	4.0f,
	TEXT(""),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> CVarTranslucencyGridEndDistanceFromCamera(
	TEXT("r.Lumen.TranslucencyVolume.EndDistanceFromCamera"),
	8000.0f,
	TEXT(""),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarTranslucencyVolumeSpatialFilter(
	TEXT("r.Lumen.TranslucencyVolume.SpatialFilter"),
	1,
	TEXT("Whether to use a spatial filter on the volume traces."),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

static TAutoConsoleVariable<int32> CVarTranslucencyVolumeSpatialFilterSampleCount(
	TEXT("r.Lumen.TranslucencyVolume.SpatialFilter.SampleCount"),
	3,
	TEXT("When r.Lumen.TranslucencyVolume.SpatialFilter.Mode=1, this controls the effective sample count of the separable filter; that will be SampleCount*2+1. Default to a [-3,3] filter of 7 sample."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> CVarTranslucencyVolumeSpatialFilterStandardDeviation(
	TEXT("r.Lumen.TranslucencyVolume.SpatialFilter.StandardDeviation"),
	5.0f, // default to a flat filter
	TEXT("When r.Lumen.TranslucencyVolume.SpatialFilter.Mode=1, The standard deviation of the Gaussian filter in Pixel. If a large value, the filter will become a cube filter. While when getting closer to 0, the filter will become a sharper Gaussian filter. Default to 5 meaning not a sharp flilter, close to a box filter for the default SampleCount of 3."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarTranslucencyVolumeTemporalReprojection(
	TEXT("r.Lumen.TranslucencyVolume.TemporalReprojection"),
	1,
	TEXT("Whether to use temporal reprojection."),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

static TAutoConsoleVariable<int32> CVarTranslucencyVolumeJitter(
	TEXT("r.Lumen.TranslucencyVolume.Temporal.Jitter"),
	1,
	TEXT("Whether to apply jitter to each frame's translucency GI computation, achieving temporal super sampling."),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

static TAutoConsoleVariable<float> CVarTranslucencyVolumeHistoryWeight(
	TEXT("r.Lumen.TranslucencyVolume.Temporal.HistoryWeight"),
	0.9,
	TEXT("How much the history value should be weighted each frame.  This is a tradeoff between visible jittering and responsiveness."),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

TAutoConsoleVariable<int32> CVarLumenTranslucencyVolumeTemporalMaxRayDirections(
	TEXT("r.Lumen.TranslucencyVolume.Temporal.MaxRayDirections"),
	8,
	TEXT("Number of possible random directions from froxel center when sampling the lumen scene."),
	ECVF_Scalability | ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> CVarTranslucencyVolumeTraceStepFactor(
	TEXT("r.Lumen.TranslucencyVolume.TraceStepFactor"),
	2,
	TEXT("."),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

static TAutoConsoleVariable<int32> CVarTranslucencyVolumeTracingOctahedronResolution(
	TEXT("r.Lumen.TranslucencyVolume.TracingOctahedronResolution"),
	3,
	TEXT("Resolution of the tracing octahedron.  Determines how many traces are done per voxel of the translucency lighting volume."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> CVarTranslucencyVolumeVoxelTraceStartDistanceScale(
	TEXT("r.Lumen.TranslucencyVolume.VoxelTraceStartDistanceScale"),
	1.0f,
	TEXT("."),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

static TAutoConsoleVariable<float> CVarTranslucencyVolumeMaxRayIntensity(
	TEXT("r.Lumen.TranslucencyVolume.MaxRayIntensity"),
	20.0f,
	TEXT("."),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

static TAutoConsoleVariable<int32> CVarLumenTranslucencyVolumeRadianceCache(
	TEXT("r.Lumen.TranslucencyVolume.RadianceCache"),
	1,
	TEXT("Whether to use the Radiance Cache for Translucency"),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

static TAutoConsoleVariable<int32> CVarTranslucencyVolumeRadianceCacheNumMipmaps(
	TEXT("r.Lumen.TranslucencyVolume.RadianceCache.NumMipmaps"),
	3,
	TEXT("Number of radiance cache mipmaps."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> CVarLumenTranslucencyVolumeRadianceCacheClipmapWorldExtent(
	TEXT("r.Lumen.TranslucencyVolume.RadianceCache.ClipmapWorldExtent"),
	2500.0f,
	TEXT("World space extent of the first clipmap"),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> CVarLumenTranslucencyVolumeRadianceCacheClipmapDistributionBase(
	TEXT("r.Lumen.TranslucencyVolume.RadianceCache.ClipmapDistributionBase"),
	2.0f,
	TEXT("Base of the Pow() that controls the size of each successive clipmap relative to the first."),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarTranslucencyVolumeRadianceCacheNumProbesToTraceBudget(
	TEXT("r.Lumen.TranslucencyVolume.RadianceCache.NumProbesToTraceBudget"),
	200,
	TEXT(""),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarTranslucencyVolumeRadianceCacheGridResolution(
	TEXT("r.Lumen.TranslucencyVolume.RadianceCache.GridResolution"),
	24,
	TEXT("Resolution of the probe placement grid within each clipmap"),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarTranslucencyVolumeRadianceCacheProbeResolution(
	TEXT("r.Lumen.TranslucencyVolume.RadianceCache.ProbeResolution"),
	8,
	TEXT("Resolution of the probe's 2d radiance layout.  The number of rays traced for the probe will be ProbeResolution ^ 2"),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarTranslucencyVolumeRadianceCacheProbeAtlasResolutionInProbes(
	TEXT("r.Lumen.TranslucencyVolume.RadianceCache.ProbeAtlasResolutionInProbes"),
	128,
	TEXT("Number of probes along one dimension of the probe atlas cache texture. This controls the memory usage of the cache. Overflow currently results in incorrect rendering. Aligned to the next power of two."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> CVarTranslucencyVolumeRadianceCacheReprojectionRadiusScale(
	TEXT("r.Lumen.TranslucencyVolume.RadianceCache.ReprojectionRadiusScale"),
	10.0f,
	TEXT(""),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarTranslucencyVolumeRadianceCacheFarField(
	TEXT("r.Lumen.TranslucencyVolume.RadianceCache.FarField"),
	0,
	TEXT("Whether to trace against the FarField representation"),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarTranslucencyVolumeRadianceCacheStats(
	TEXT("r.Lumen.TranslucencyVolume.RadianceCache.Stats"),
	0,
	TEXT("GPU print out Radiance Cache update stats."),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarTranslucencyVolumeRadianceCacheFrustumProbes(
	TEXT("r.Lumen.TranslucencyVolume.RadianceCache.FrustumProbes"),
	0,
	TEXT("Enable the use of probes generated on view fruxtum froxels as radiance cache, instead of using a worls space radiance cache."),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarTranslucencyVolumeRadianceCacheFrustumLowResProbesProbeResolution(
	TEXT("r.Lumen.TranslucencyVolume.RadianceCache.FrustumProbes.LowResProbeResolution"),
	6,
	TEXT("Low resolution probes a re used to initialise the frustrum probes on camera cut or if temporal reprojection cannot happen. This is a warm up resolution before reprojection + temporal update happen. The number of rays traced for the probe will be ProbeResolution ^ 2. Must be within [4, 8]."),
	ECVF_RenderThreadSafe | ECVF_Scalability
);

static TAutoConsoleVariable<int32> CVarTranslucencyVolumeRadianceCacheFrustumProbesProbeResolution(
	TEXT("r.Lumen.TranslucencyVolume.RadianceCache.FrustumProbes.ProbeResolution"),
	16,
	TEXT("Resolution of the frustum probes's 2d radiance layout.  The number of rays traced for the probe will be ProbeResolution ^ 2. Must be within [4, 64]."),
	ECVF_RenderThreadSafe | ECVF_Scalability
);

static TAutoConsoleVariable<int32> CVarTranslucencyVolumeRadianceCacheFrustumProbesFroxelSize(
	TEXT("r.Lumen.TranslucencyVolume.RadianceCache.FrustumProbes.FroxelSize"),
	4,
	TEXT("Size of a frustum probes in the translucency froxel grid, in froxel."),
	ECVF_RenderThreadSafe | ECVF_Scalability
);

static TAutoConsoleVariable<int32> CVarTranslucencyVolumeRadianceCacheFrustumProbesRefineTracePerFrame(
	TEXT("r.Lumen.TranslucencyVolume.RadianceCache.FrustumProbes.RefineTracePerFrame"),
	2,
	TEXT("Size of a frustum probes in the translucency froxel grid, in froxel. Must be within [1, 8]."),
	ECVF_RenderThreadSafe | ECVF_Scalability
);

static TAutoConsoleVariable<int32> CVarTranslucencyVolumeRadianceCacheFrustumProbesDebug(
	TEXT("r.Lumen.TranslucencyVolume.RadianceCache.FrustumProbes.Debug"),
	0,
	TEXT("Print debug information about the trace frustum probe froxel."),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> CVarTranslucencyVolumeGridCenterOffsetFromDepthBuffer(
	TEXT("r.Lumen.TranslucencyVolume.GridCenterOffsetFromDepthBuffer"),
	0.5f,
	TEXT("Offset in grid units to move grid center sample out form the depth buffer along the Z direction. -1 means disabled. This reduces sample self intersection with geometry when tracing the global distance field buffer, and thus reduces flickering in those areas, as well as results in less leaking sometimes. Set to -1 to disable."),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> CVarTranslucencyVolumeOffsetThresholdToAcceptDepthBufferOffset(
	TEXT("r.Lumen.TranslucencyVolume.OffsetThresholdToAcceptDepthBufferOffset"),
	1.0f,
	TEXT("Offset in grid units to accept a sample to be moved forward in front of the depth buffer. This is to avoid moving all samples behind the depth buffer forward which would affect the lighting of translucent and volumetric at edges of mesh. Default to 1.0 to only allow moving the first layer of froxel intersecting depth."),
	ECVF_RenderThreadSafe
);

namespace LumenTranslucencyVolume
{
	float GetEndDistanceFromCamera(const FViewInfo& View)
	{
		// Ideally we'd use LumenSceneViewDistance directly, but direct shadowing via translucency lighting volume only covers 5000.0f units by default (r.TranslucencyLightingVolumeOuterDistance), 
		//		so there isn't much point covering beyond that.  
		const float ViewDistanceScale = FMath::Clamp(View.FinalPostProcessSettings.LumenSceneViewDistance / 20000.0f, .1f, 100.0f);
		return FMath::Clamp<float>(CVarTranslucencyGridEndDistanceFromCamera.GetValueOnRenderThread() * ViewDistanceScale, 1.0f, 100000.0f);
	}
}

namespace LumenTranslucencyVolumeRadianceCache
{
	int32 GetNumClipmaps(float DistanceToCover)
	{
		int32 ClipmapIndex = 0;

		for (; ClipmapIndex < LumenRadianceCache::MaxClipmaps; ++ClipmapIndex)
		{
			const float ClipmapExtent = CVarLumenTranslucencyVolumeRadianceCacheClipmapWorldExtent.GetValueOnRenderThread() * FMath::Pow(CVarLumenTranslucencyVolumeRadianceCacheClipmapDistributionBase.GetValueOnRenderThread(), ClipmapIndex);

			if (ClipmapExtent > DistanceToCover)
			{
				break;
			}
		}

		return FMath::Clamp(ClipmapIndex + 1, 1, LumenRadianceCache::MaxClipmaps);
	}

	int32 GetClipmapGridResolution()
	{
		const int32 GridResolution = CVarTranslucencyVolumeRadianceCacheGridResolution.GetValueOnRenderThread();
		return FMath::Clamp(GridResolution, 1, 256);
	}

	int32 GetProbeResolution()
	{
		return CVarTranslucencyVolumeRadianceCacheProbeResolution.GetValueOnRenderThread();
	}

	int32 GetNumMipmaps()
	{
		return CVarTranslucencyVolumeRadianceCacheNumMipmaps.GetValueOnRenderThread();
	}

	int32 GetFinalProbeResolution()
	{
		return GetProbeResolution() + 2 * (1 << (GetNumMipmaps() - 1));
	}

	int32 GetProbeAtlasResolutionInProbes()
	{
		return FMath::RoundUpToPowerOfTwo(FMath::Clamp(CVarTranslucencyVolumeRadianceCacheProbeAtlasResolutionInProbes.GetValueOnRenderThread(), 1, 1024));
	}

	LumenRadianceCache::FRadianceCacheInputs SetupRadianceCacheInputs(const FViewInfo& View)
	{
		LumenRadianceCache::FRadianceCacheInputs Parameters = LumenRadianceCache::GetDefaultRadianceCacheInputs();
		Parameters.ReprojectionRadiusScale = CVarTranslucencyVolumeRadianceCacheReprojectionRadiusScale.GetValueOnRenderThread();
		Parameters.ClipmapWorldExtent = CVarLumenTranslucencyVolumeRadianceCacheClipmapWorldExtent.GetValueOnRenderThread();
		Parameters.ClipmapDistributionBase = CVarLumenTranslucencyVolumeRadianceCacheClipmapDistributionBase.GetValueOnRenderThread();
		Parameters.RadianceProbeClipmapResolution = GetClipmapGridResolution();
		Parameters.ProbeAtlasResolutionInProbes = FIntPoint(GetProbeAtlasResolutionInProbes(), GetProbeAtlasResolutionInProbes());
		Parameters.NumRadianceProbeClipmaps = GetNumClipmaps(LumenTranslucencyVolume::GetEndDistanceFromCamera(View));
		Parameters.RadianceProbeResolution = FMath::Max(GetProbeResolution(), LumenRadianceCache::MinRadianceProbeResolution);
		Parameters.FinalProbeResolution = GetFinalProbeResolution();
		Parameters.FinalRadianceAtlasMaxMip = GetNumMipmaps() - 1;
		const float TraceBudgetScale = View.Family->bCurrentlyBeingEdited ? 10.0f : 1.0f;
		Parameters.NumProbesToTraceBudget = CVarTranslucencyVolumeRadianceCacheNumProbesToTraceBudget.GetValueOnRenderThread() * TraceBudgetScale;
		Parameters.RadianceCacheStats = CVarTranslucencyVolumeRadianceCacheStats.GetValueOnRenderThread();
		return Parameters;
	}
};

static bool GetVolumeRadianceCacheFrustumProbes()
{
	return CVarTranslucencyVolumeRadianceCacheFrustumProbes.GetValueOnRenderThread() > 0;
}

const static uint32 MaxTranslucencyVolumeConeDirections = 64;

FRDGTextureRef OrDefault2dTextureIfNull(FRDGBuilder& GraphBuilder, FRDGTextureRef Texture)
{
	return Texture ? Texture : GSystemTextures.GetBlackDummy(GraphBuilder);
}

FRDGTextureRef OrDefault2dArrayTextureIfNull(FRDGBuilder& GraphBuilder, FRDGTextureRef Texture)
{
    return Texture ? Texture : GSystemTextures.GetBlackArrayDummy(GraphBuilder);
}

FRDGTextureRef OrDefault3dTextureIfNull(FRDGBuilder& GraphBuilder, FRDGTextureRef Texture)
{
	return Texture ? Texture : GSystemTextures.GetVolumetricBlackDummy(GraphBuilder);
}

FRDGTextureRef OrDefault3dUintTextureIfNull(FRDGBuilder& GraphBuilder, FRDGTextureRef Texture)
{
	return Texture ? Texture: GSystemTextures.GetVolumetricBlackUintDummy(GraphBuilder);
}

float GetLumenReflectionSpecularScale();
float GetLumenReflectionContrast();
FLumenTranslucencyLightingParameters GetLumenTranslucencyLightingParameters(
	FRDGBuilder& GraphBuilder, 
	const FLumenTranslucencyGIVolume& LumenTranslucencyGIVolume,
	const FLumenFrontLayerTranslucency& LumenFrontLayerTranslucency)
{
	const FRDGSystemTextures& SystemTextures = FRDGSystemTextures::Get(GraphBuilder);

	FLumenTranslucencyLightingParameters Parameters;
	Parameters.RadianceCacheInterpolationParameters = LumenTranslucencyGIVolume.RadianceCacheInterpolationParameters;

	if (!LumenTranslucencyGIVolume.RadianceCacheInterpolationParameters.RadianceCacheFinalRadianceAtlas)
	{
		Parameters.RadianceCacheInterpolationParameters.RadianceCacheInputs.FinalProbeResolution = 0;
	}

	Parameters.RadianceCacheInterpolationParameters.RadianceProbeIndirectionTexture = OrDefault3dUintTextureIfNull(GraphBuilder, Parameters.RadianceCacheInterpolationParameters.RadianceProbeIndirectionTexture);
	Parameters.RadianceCacheInterpolationParameters.RadianceCacheFinalRadianceAtlas = OrDefault2dTextureIfNull(GraphBuilder, Parameters.RadianceCacheInterpolationParameters.RadianceCacheFinalRadianceAtlas);
	Parameters.RadianceCacheInterpolationParameters.RadianceCacheFinalIrradianceAtlas = OrDefault2dTextureIfNull(GraphBuilder, Parameters.RadianceCacheInterpolationParameters.RadianceCacheFinalIrradianceAtlas);
	Parameters.RadianceCacheInterpolationParameters.RadianceCacheProbeOcclusionAtlas = OrDefault2dTextureIfNull(GraphBuilder, Parameters.RadianceCacheInterpolationParameters.RadianceCacheProbeOcclusionAtlas);
	Parameters.RadianceCacheInterpolationParameters.RadianceCacheDepthAtlas = OrDefault2dTextureIfNull(GraphBuilder, Parameters.RadianceCacheInterpolationParameters.RadianceCacheDepthAtlas);
	
	if (!Parameters.RadianceCacheInterpolationParameters.ProbeWorldOffset)
	{
		Parameters.RadianceCacheInterpolationParameters.ProbeWorldOffset = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(GSystemTextures.GetDefaultStructuredBuffer(GraphBuilder, sizeof(FVector4f))));
	}

	Parameters.FrontLayerTranslucencyReflectionParameters.Enabled = LumenFrontLayerTranslucency.bEnabled ? 1 : 0;
	Parameters.FrontLayerTranslucencyReflectionParameters.RelativeDepthThreshold = LumenFrontLayerTranslucency.RelativeDepthThreshold;
	Parameters.FrontLayerTranslucencyReflectionParameters.Radiance = OrDefault2dArrayTextureIfNull(GraphBuilder, LumenFrontLayerTranslucency.Radiance);
	Parameters.FrontLayerTranslucencyReflectionParameters.Normal = OrDefault2dTextureIfNull(GraphBuilder, LumenFrontLayerTranslucency.Normal);
	Parameters.FrontLayerTranslucencyReflectionParameters.SceneDepth = OrDefault2dTextureIfNull(GraphBuilder, LumenFrontLayerTranslucency.SceneDepth);
	Parameters.FrontLayerTranslucencyReflectionParameters.SpecularScale = GetLumenReflectionSpecularScale();
	Parameters.FrontLayerTranslucencyReflectionParameters.Contrast = GetLumenReflectionContrast();

	Parameters.TranslucencyGIVolume0            = LumenTranslucencyGIVolume.Texture0        ? LumenTranslucencyGIVolume.Texture0        : SystemTextures.VolumetricBlack;
	Parameters.TranslucencyGIVolume1            = LumenTranslucencyGIVolume.Texture1        ? LumenTranslucencyGIVolume.Texture1        : SystemTextures.VolumetricBlack;
	Parameters.TranslucencyGIVolumeHistory0     = LumenTranslucencyGIVolume.HistoryTexture0 ? LumenTranslucencyGIVolume.HistoryTexture0 : SystemTextures.VolumetricBlack;
	Parameters.TranslucencyGIVolumeHistory1     = LumenTranslucencyGIVolume.HistoryTexture1 ? LumenTranslucencyGIVolume.HistoryTexture1 : SystemTextures.VolumetricBlack;
	Parameters.TranslucencyGIVolumeSampler      = TStaticSamplerState<SF_Trilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	Parameters.TranslucencyGIGridZParams        = (FVector3f)LumenTranslucencyGIVolume.GridZParams;
	Parameters.TranslucencyGIGridPixelSizeShift = LumenTranslucencyGIVolume.GridPixelSizeShift;
	Parameters.TranslucencyGIGridSize           = LumenTranslucencyGIVolume.GridSize;
	return Parameters;
}

void GetTranslucencyGridZParams(float NearPlane, float FarPlane, FVector& OutZParams, int32& OutGridSizeZ)
{
	OutGridSizeZ = FMath::TruncToInt(FMath::Log2((FarPlane - NearPlane) * CVarTranslucencyGridDistributionLogZScale.GetValueOnRenderThread()) * CVarTranslucencyGridDistributionZScale.GetValueOnRenderThread()) + 1;
	OutZParams = FVector(CVarTranslucencyGridDistributionLogZScale.GetValueOnRenderThread(), CVarTranslucencyGridDistributionLogZOffset.GetValueOnRenderThread(), CVarTranslucencyGridDistributionZScale.GetValueOnRenderThread());
}

FVector TranslucencyVolumeTemporalRandom(uint32 FrameNumber)
{
	// Center of the voxel
	FVector RandomOffsetValue(.5f, .5f, .5f);

	if (CVarTranslucencyVolumeJitter.GetValueOnRenderThread())
	{
		RandomOffsetValue = FVector(Halton(FrameNumber & 1023, 2), Halton(FrameNumber & 1023, 3), Halton(FrameNumber & 1023, 5));
	}

	return RandomOffsetValue;
}


class FMarkRadianceProbesUsedByTranslucencyVolumeCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FMarkRadianceProbesUsedByTranslucencyVolumeCS)
	SHADER_USE_PARAMETER_STRUCT(FMarkRadianceProbesUsedByTranslucencyVolumeCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_STRUCT_INCLUDE(LumenRadianceCache::FRadianceCacheMarkParameters, RadianceCacheMarkParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(FLumenTranslucencyLightingVolumeParameters, VolumeParameters)
	END_SHADER_PARAMETER_STRUCT()

	using FPermutationDomain = TShaderPermutationDomain<>;

	static FIntVector GetGroupSize()
	{
		return FIntVector(4, 4, 4);
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportLumenGI(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize().X);
		OutEnvironment.CompilerFlags.Add(CFLAG_Wave32);
	}
};

IMPLEMENT_GLOBAL_SHADER(FMarkRadianceProbesUsedByTranslucencyVolumeCS, "/Engine/Private/Lumen/LumenTranslucencyVolumeLighting.usf", "MarkRadianceProbesUsedByTranslucencyVolumeCS", SF_Compute);


class FTranslucencyVolumeTraceVoxelsCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FTranslucencyVolumeTraceVoxelsCS)
	SHADER_USE_PARAMETER_STRUCT(FTranslucencyVolumeTraceVoxelsCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FLumenCardTracingParameters, TracingParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(LumenRadianceCache::FRadianceCacheInterpolationParameters, RadianceCacheParameters)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D<float3>, RWVolumeTraceRadiance)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D<float>, RWVolumeTraceHitDistance)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float3>, VolumeFroxelProbeRadianceHitDistance)
		SHADER_PARAMETER_STRUCT_INCLUDE(FLumenTranslucencyLightingVolumeParameters, VolumeParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(FLumenTranslucencyLightingVolumeTraceSetupParameters, TraceSetupParameters)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSceneTextureUniformParameters, SceneTexturesStruct)
	END_SHADER_PARAMETER_STRUCT()

	class FDynamicSkyLight : SHADER_PERMUTATION_BOOL("ENABLE_DYNAMIC_SKY_LIGHT");
	class FProbeSourceMode : SHADER_PERMUTATION_RANGE_INT("PROBE_SOURCE_MODE", 0, 3);
	class FTraceFromVolume : SHADER_PERMUTATION_BOOL("TRACE_FROM_VOLUME");
	class FSimpleCoverageBasedExpand : SHADER_PERMUTATION_BOOL("GLOBALSDF_SIMPLE_COVERAGE_BASED_EXPAND");

	using FPermutationDomain = TShaderPermutationDomain<FDynamicSkyLight, FProbeSourceMode, FTraceFromVolume, FSimpleCoverageBasedExpand>;

	static FIntVector GetGroupSize()
	{
		return FIntVector(8, 8, 1);
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		const FPermutationDomain PermutationVector(Parameters.PermutationId);

		if (!PermutationVector.Get<FTraceFromVolume>() && PermutationVector.Get<FSimpleCoverageBasedExpand>())
		{
			return false;
		}

		return DoesPlatformSupportLumenGI(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize().X);
		OutEnvironment.CompilerFlags.Add(CFLAG_Wave32);
	}
};

IMPLEMENT_GLOBAL_SHADER(FTranslucencyVolumeTraceVoxelsCS, "/Engine/Private/Lumen/LumenTranslucencyVolumeLighting.usf", "TranslucencyVolumeTraceVoxelsCS", SF_Compute);


#define MAX_FRUSTUM_PROBES_REFINE_TRACEPERFRAME 8

class FFroxelProbesUpdateSchedulerCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FFroxelProbesUpdateSchedulerCS)
	SHADER_USE_PARAMETER_STRUCT(FFroxelProbesUpdateSchedulerCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_STRUCT_INCLUDE(FLumenTranslucencyLightingVolumeParameters, VolumeParameters)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWByteAddressBuffer, FroxelClearCountBufferUAV)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint4>, FroxelClearListBufferUAV)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWByteAddressBuffer, FroxelInitAndRefineCountBufferUAV)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint2>, FroxelLowResInitListBufferUAV)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint4>, FroxelRefineListBufferUAV)
		SHADER_PARAMETER_ARRAY(FUintVector4, ProbeSamplesToTrace, [MAX_FRUSTUM_PROBES_REFINE_TRACEPERFRAME])
		SHADER_PARAMETER(FMatrix44f, UnjitteredPrevWorldToClip)
		SHADER_PARAMETER(uint32, CameraCut)
	END_SHADER_PARAMETER_STRUCT()

	using FPermutationDomain = TShaderPermutationDomain<>;

	static FIntVector GetGroupSize()
	{
		return FIntVector(8, 8, 1);
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		const FPermutationDomain PermutationVector(Parameters.PermutationId);
		return DoesPlatformSupportLumenGI(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize().X);
	}
};

IMPLEMENT_GLOBAL_SHADER(FFroxelProbesUpdateSchedulerCS, "/Engine/Private/Lumen/LumenTranslucencyVolumeLighting.usf", "FroxelProbesUpdateSchedulerMainCS", SF_Compute);


class FFroxelProbesUpdateIndirectArgsSetupCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FFroxelProbesUpdateIndirectArgsSetupCS)
	SHADER_USE_PARAMETER_STRUCT(FFroxelProbesUpdateIndirectArgsSetupCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FLumenTranslucencyLightingVolumeParameters, VolumeParameters)
		SHADER_PARAMETER_RDG_BUFFER_SRV(ByteAddressBuffer, FroxelClearCountBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, FroxelClearDispatchIndirectBufferUAV)
		SHADER_PARAMETER_RDG_BUFFER_SRV(ByteAddressBuffer, FroxelInitAndRefineCountBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, FroxelInitAndRefineTraceDispatchIndirectBufferUAV)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, FroxelLowResCopyDispatchIndirectBufferUAV)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, FroxelReprojRefineDispatchIndirectBufferUAV)
		SHADER_PARAMETER_STRUCT_INCLUDE(ShaderPrint::FShaderParameters, ShaderPrintUniformBuffer)
		SHADER_PARAMETER(FIntPoint, RayTracingThreadGroupSize)
		SHADER_PARAMETER(uint32, bUsingRayTracing)
	END_SHADER_PARAMETER_STRUCT()
		
	class FDebugPrint : SHADER_PERMUTATION_BOOL("PERMUTATION_DEBUG_PRINT");
	using FPermutationDomain = TShaderPermutationDomain<FDebugPrint>;

	static FIntVector GetGroupSize()
	{
		return FIntVector(8, 1, 1);
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		const FPermutationDomain PermutationVector(Parameters.PermutationId);
		return DoesPlatformSupportLumenGI(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize().X);

		const FPermutationDomain PermutationVector(Parameters.PermutationId);
		if (PermutationVector.Get<FDebugPrint>())
		{
			ShaderPrint::ModifyCompilationEnvironment(Parameters.Platform, OutEnvironment);
		}
	}
};

IMPLEMENT_GLOBAL_SHADER(FFroxelProbesUpdateIndirectArgsSetupCS, "/Engine/Private/Lumen/LumenTranslucencyVolumeLighting.usf", "FroxelProbesUpdateIndirectArgsSetupMainCS", SF_Compute);


class FUpdateFroxelProbesCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FUpdateFroxelProbesCS)
	SHADER_USE_PARAMETER_STRUCT(FUpdateFroxelProbesCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float4>, VolumeFroxelProbeRadianceHitDistanceHistory)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float3>, VolumeFroxelLowResProbeRadiance)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>,  VolumeFroxelLowResProbeHitDistance)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D<float4>, RWVolumeFroxelProbeRadianceHitDistance)
		SHADER_PARAMETER_STRUCT_INCLUDE(FLumenTranslucencyLightingVolumeParameters, VolumeParameters)
		SHADER_PARAMETER_RDG_BUFFER_SRV(ByteAddressBuffer, FroxelClearCountBuffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(ByteAddressBuffer, FroxelInitAndRefineCountBuffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint4>, FroxelUpdateListBuffer)
		RDG_BUFFER_ACCESS(DispatchIndirectBuffer, ERHIAccess::IndirectArgs)
		SHADER_PARAMETER_ARRAY(FUintVector4, ProbeSamplesToTrace, [MAX_FRUSTUM_PROBES_REFINE_TRACEPERFRAME])
		SHADER_PARAMETER(FMatrix44f, UnjitteredPrevWorldToClip)
		SHADER_PARAMETER(FVector2f, ViewFroxelProbesHistoryPreExposureAndInv)
	END_SHADER_PARAMETER_STRUCT()
		
	class FProbeFillMode : SHADER_PERMUTATION_RANGE_INT("PERMUTATION_PROBE_FILL_MODE", 0, 3); // 0 is for reprojection, 1 is for reset using low res probe, 2 clear probe
	using FPermutationDomain = TShaderPermutationDomain<FProbeFillMode>;

	static FIntVector GetGroupSize()
	{
		return FIntVector(4, 4, 4);
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		const FPermutationDomain PermutationVector(Parameters.PermutationId);
		return DoesPlatformSupportLumenGI(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.CompilerFlags.Add(CFLAG_Wave32);
	}
};

IMPLEMENT_GLOBAL_SHADER(FUpdateFroxelProbesCS, "/Engine/Private/Lumen/LumenTranslucencyVolumeLighting.usf", "UpdateFroxelProbesMainCS", SF_Compute);


class FTranslucencyVolumeFroxelProbesSoftwareRayTracingCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FTranslucencyVolumeFroxelProbesSoftwareRayTracingCS)
	SHADER_USE_PARAMETER_STRUCT(FTranslucencyVolumeFroxelProbesSoftwareRayTracingCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FLumenCardTracingParameters, TracingParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(LumenRadianceCache::FRadianceCacheInterpolationParameters, RadianceCacheParameters)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D<float3>, RWVolumeTraceRadiance)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D<float>, RWVolumeTraceHitDistance)
		SHADER_PARAMETER_STRUCT_INCLUDE(FLumenTranslucencyLightingVolumeParameters, VolumeParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(FLumenTranslucencyLightingVolumeTraceSetupParameters, TraceSetupParameters)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSceneTextureUniformParameters, SceneTexturesStruct)
		SHADER_PARAMETER_RDG_BUFFER_SRV(ByteAddressBuffer, FroxelInitAndRefineCountBuffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint4>, FroxelLowResInitListBuffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint4>, FroxelRefineListBufferSRV)
		RDG_BUFFER_ACCESS(FroxelProbeRayTraceDispatchIndirectBuffer, ERHIAccess::IndirectArgs)
	END_SHADER_PARAMETER_STRUCT()

	class FDynamicSkyLight : SHADER_PERMUTATION_BOOL("ENABLE_DYNAMIC_SKY_LIGHT");
	class FSimpleCoverageBasedExpand : SHADER_PERMUTATION_BOOL("GLOBALSDF_SIMPLE_COVERAGE_BASED_EXPAND");
	using FPermutationDomain = TShaderPermutationDomain<FDynamicSkyLight, FSimpleCoverageBasedExpand>;

	static FIntVector GetGroupSize()
	{
		return FIntVector(64, 1, 1);
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		const FPermutationDomain PermutationVector(Parameters.PermutationId);
		return DoesPlatformSupportLumenGI(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize().X);
		OutEnvironment.CompilerFlags.Add(CFLAG_Wave32);
	}
};

IMPLEMENT_GLOBAL_SHADER(FTranslucencyVolumeFroxelProbesSoftwareRayTracingCS, "/Engine/Private/Lumen/LumenTranslucencyVolumeLighting.usf", "TranslucencyVolumeFroxelProbesSoftwareRayTracingCS", SF_Compute);


#if RHI_RAYTRACING

class FTranslucencyVolumeFroxelProbesHardwareRayTracing : public FLumenHardwareRayTracingShaderBase
{
	DECLARE_LUMEN_RAYTRACING_SHADER(FTranslucencyVolumeFroxelProbesHardwareRayTracing)

	class FDynamicSkyLight : SHADER_PERMUTATION_BOOL("ENABLE_DYNAMIC_SKY_LIGHT");
	using FPermutationDomain = TShaderPermutationDomain<FLumenHardwareRayTracingShaderBase::FBasePermutationDomain, FDynamicSkyLight>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D<float3>, RWVolumeTraceRadiance)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D<float>, RWVolumeTraceHitDistance)
		SHADER_PARAMETER_STRUCT_INCLUDE(FLumenHardwareRayTracingShaderBase::FSharedParameters, SharedParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(LumenRadianceCache::FRadianceCacheInterpolationParameters, RadianceCacheParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(FLumenTranslucencyLightingVolumeParameters, VolumeParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(FLumenTranslucencyLightingVolumeTraceSetupParameters, TraceSetupParameters)
		SHADER_PARAMETER_RDG_BUFFER_SRV(ByteAddressBuffer, FroxelInitAndRefineCountBuffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint4>, FroxelLowResInitListBuffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint4>, FroxelRefineListBufferSRV)
		RDG_BUFFER_ACCESS(FroxelProbeRayTraceDispatchIndirectBuffer, ERHIAccess::IndirectArgs)
	END_SHADER_PARAMETER_STRUCT()

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, Lumen::ERayTracingShaderDispatchType ShaderDispatchType, FShaderCompilerEnvironment& OutEnvironment)
	{
		FLumenHardwareRayTracingShaderBase::ModifyCompilationEnvironment(Parameters, ShaderDispatchType, Lumen::ESurfaceCacheSampling::AlwaysResidentPagesWithoutFeedback, OutEnvironment);
	}

	static ERayTracingPayloadType GetRayTracingPayloadType(const int32 PermutationId)
	{
		return ERayTracingPayloadType::LumenMinimal;
	}
};

IMPLEMENT_LUMEN_RAYGEN_AND_COMPUTE_RAYTRACING_SHADERS(FTranslucencyVolumeFroxelProbesHardwareRayTracing)

IMPLEMENT_GLOBAL_SHADER(FTranslucencyVolumeFroxelProbesHardwareRayTracingRGS, "/Engine/Private/Lumen/LumenTranslucencyVolumeLighting.usf", "TranslucencyVolumeFroxelProbesHardwareRayTracingRGS", SF_RayGen);
IMPLEMENT_GLOBAL_SHADER(FTranslucencyVolumeFroxelProbesHardwareRayTracingCS,  "/Engine/Private/Lumen/LumenTranslucencyVolumeLighting.usf", "TranslucencyVolumeFroxelProbesHardwareRayTracingCS",  SF_Compute);

#endif // RHI_RAYTRACING


class FTranslucencyVolumeSpatialSeparableFilterCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FTranslucencyVolumeSpatialSeparableFilterCS)
	SHADER_USE_PARAMETER_STRUCT(FTranslucencyVolumeSpatialSeparableFilterCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D<float3>, RWVolumeTraceRadiance)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D, VolumeTraceRadiance)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D, VolumeTraceHitDistance)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_STRUCT_INCLUDE(FLumenTranslucencyLightingVolumeParameters, VolumeParameters)
		SHADER_PARAMETER(FVector3f, PreviousFrameJitterOffset)
		SHADER_PARAMETER(FMatrix44f, UnjitteredPrevWorldToClip)
		SHADER_PARAMETER(FIntVector3, SpatialFilterDirection)
		SHADER_PARAMETER(FVector3f, SpatialFilterGaussParams)
		SHADER_PARAMETER(int32, SpatialFilterSampleCount)
	END_SHADER_PARAMETER_STRUCT()

	using FPermutationDomain = TShaderPermutationDomain<>;

	static FIntVector GetGroupSize()
	{
		return FIntVector(8, 8, 1);
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportLumenGI(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize().X);
	}
};

IMPLEMENT_GLOBAL_SHADER(FTranslucencyVolumeSpatialSeparableFilterCS, "/Engine/Private/Lumen/LumenTranslucencyVolumeLighting.usf", "TranslucencyVolumeSpatialSeparableFilterCS", SF_Compute);


class FTranslucencyVolumeIntegrateCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FTranslucencyVolumeIntegrateCS)
	SHADER_USE_PARAMETER_STRUCT(FTranslucencyVolumeIntegrateCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D<float4>, RWTranslucencyGI0)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D<float4>, RWTranslucencyGI1)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D<float4>, RWTranslucencyGINewHistory0)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D<float4>, RWTranslucencyGINewHistory1)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_STRUCT_INCLUDE(FLumenTranslucencyLightingVolumeParameters, VolumeParameters)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D, VolumeTraceRadiance)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D, VolumeTraceHitDistance)
		SHADER_PARAMETER(float, HistoryWeight)
		SHADER_PARAMETER(FVector3f, PreviousFrameJitterOffset)
		SHADER_PARAMETER(FMatrix44f, UnjitteredPrevWorldToClip)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D, TranslucencyGIHistory0)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D, TranslucencyGIHistory1)
		SHADER_PARAMETER_SAMPLER(SamplerState, TranslucencyGIHistorySampler)
	END_SHADER_PARAMETER_STRUCT()

	class FTemporalReprojection : SHADER_PERMUTATION_BOOL("USE_TEMPORAL_REPROJECTION");

	using FPermutationDomain = TShaderPermutationDomain<FTemporalReprojection>;

	static FIntVector GetGroupSize()
	{
		return FIntVector(4, 4, 4);
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportLumenGI(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize().X);
	}
};

IMPLEMENT_GLOBAL_SHADER(FTranslucencyVolumeIntegrateCS, "/Engine/Private/Lumen/LumenTranslucencyVolumeLighting.usf", "TranslucencyVolumeIntegrateCS", SF_Compute);

FLumenTranslucencyLightingVolumeParameters GetTranslucencyLightingVolumeParameters(const FViewInfo& View)
{
	const int32 TranslucencyFroxelGridPixelSize = FMath::Max(1, CVarTranslucencyFroxelGridPixelSize.GetValueOnRenderThread());
	const FIntPoint GridSizeXY = FIntPoint::DivideAndRoundUp(View.ViewRect.Size(), TranslucencyFroxelGridPixelSize);
	const float FarPlane = LumenTranslucencyVolume::GetEndDistanceFromCamera(View);
	const uint32 ViewStateFrameIndex = View.ViewState ? View.ViewState->GetFrameIndex() : 0;

	FVector ZParams;
	int32 GridSizeZ;
	GetTranslucencyGridZParams(View.NearClippingDistance, FarPlane, ZParams, GridSizeZ);

	const FIntVector TranslucencyGridSize(GridSizeXY.X, GridSizeXY.Y, FMath::Max(GridSizeZ, 1));

	FLumenTranslucencyLightingVolumeParameters Parameters;
	Parameters.TranslucencyGIGridZParams = (FVector3f)ZParams;
	Parameters.TranslucencyGIGridPixelSizeShift = FMath::FloorLog2(TranslucencyFroxelGridPixelSize);
	Parameters.TranslucencyGIGridSize = TranslucencyGridSize;

	Parameters.FrameJitterOffset = (FVector3f)TranslucencyVolumeTemporalRandom(ViewStateFrameIndex);
	Parameters.UnjitteredClipToTranslatedWorld = FMatrix44f(View.ViewMatrices.ComputeInvProjectionNoAAMatrix() * View.ViewMatrices.GetTranslatedViewMatrix().GetTransposed());		// LWC_TODO: Precision loss?
	Parameters.GridCenterOffsetFromDepthBuffer = CVarTranslucencyVolumeGridCenterOffsetFromDepthBuffer.GetValueOnRenderThread();
	Parameters.GridCenterOffsetThresholdToAcceptDepthBufferOffset = FMath::Max(0, CVarTranslucencyVolumeOffsetThresholdToAcceptDepthBufferOffset.GetValueOnRenderThread());
	Parameters.FroxelDirectionJitterFrameIndex = CVarTranslucencyVolumeJitter.GetValueOnRenderThread() ? int32(ViewStateFrameIndex % FMath::Max(1, CVarLumenTranslucencyVolumeTemporalMaxRayDirections.GetValueOnRenderThread())) : -1;

	Parameters.BlueNoise = CreateUniformBufferImmediate(GetBlueNoiseGlobalParameters(), EUniformBufferUsage::UniformBuffer_SingleDraw);
		
	Parameters.TranslucencyVolumeTracingOctahedronResolution = CVarTranslucencyVolumeTracingOctahedronResolution.GetValueOnRenderThread();

	// Froxel probes
	Parameters.TranslucencyVolumeTracingFroxelLowResProbesOctahedronResolution = FMath::Clamp(CVarTranslucencyVolumeRadianceCacheFrustumLowResProbesProbeResolution.GetValueOnRenderThread(), 2, 8);
	Parameters.TranslucencyVolumeTracingFroxelProbesOctahedronResolution = FMath::Clamp(CVarTranslucencyVolumeRadianceCacheFrustumProbesProbeResolution.GetValueOnRenderThread(), 4, 64);
	const uint32 FrustomProbeFroxelSize = FMath::Max(1u, (uint32)CVarTranslucencyVolumeRadianceCacheFrustumProbesFroxelSize.GetValueOnRenderThread());
	Parameters.TranslucencyVolumeTracingFroxelProbesFroxelSize = FUintVector(FrustomProbeFroxelSize, FrustomProbeFroxelSize, 1u); // No reduction of probe placement along depth
	Parameters.TranslucencyVolumeTracingFroxelProbesGridSize = FUintVector::DivideAndRoundUp(FUintVector(TranslucencyGridSize), Parameters.TranslucencyVolumeTracingFroxelProbesFroxelSize);
	Parameters.TranslucencyVolumeTracingFroxelProbePixelSizeShift = FMath::FloorLog2(TranslucencyFroxelGridPixelSize * FrustomProbeFroxelSize);
	Parameters.TranslucencyVolumeTracingFroxelProbeHZBMipLevel = FMath::Max<float>((int32)FMath::FloorLog2(TranslucencyFroxelGridPixelSize * FrustomProbeFroxelSize) - 1, 0.0f);
	Parameters.TranslucencyVolumeTracingFroxelProbeRefineTraceCountPerFrame = 
		FMath::Min((uint32)FMath::Clamp(CVarTranslucencyVolumeRadianceCacheFrustumProbesRefineTracePerFrame.GetValueOnRenderThread(), 1, 8), 
			Parameters.TranslucencyVolumeTracingFroxelLowResProbesOctahedronResolution);	//Refine traces are put in the first row of each low res probe. So we clamp so the size at maximum (otherwise we need to handle packing on multiple rows)
	
	Parameters.FurthestHZBTexture = View.HZB;
	Parameters.HZBMipLevel = FMath::Max<float>((int32)FMath::FloorLog2(TranslucencyFroxelGridPixelSize) - 1, 0.0f);
	Parameters.ViewportUVToHZBBufferUV = FVector2f(
		float(View.ViewRect.Width()) / float(2 * View.HZBMipmap0Size.X),
		float(View.ViewRect.Height()) / float(2 * View.HZBMipmap0Size.Y));

	return Parameters;
}

static void MarkRadianceProbesUsedByTranslucencyVolume(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View,
	FLumenTranslucencyLightingVolumeParameters VolumeParameters,
	const LumenRadianceCache::FRadianceCacheMarkParameters& RadianceCacheMarkParameters,
	ERDGPassFlags ComputePassFlags)
{
	FMarkRadianceProbesUsedByTranslucencyVolumeCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FMarkRadianceProbesUsedByTranslucencyVolumeCS::FParameters>();
	PassParameters->View = View.ViewUniformBuffer;
	PassParameters->RadianceCacheMarkParameters = RadianceCacheMarkParameters;

	PassParameters->VolumeParameters = VolumeParameters;

	FMarkRadianceProbesUsedByTranslucencyVolumeCS::FPermutationDomain PermutationVector;
	auto ComputeShader = View.ShaderMap->GetShader<FMarkRadianceProbesUsedByTranslucencyVolumeCS>();

	const FIntVector GroupSize = FComputeShaderUtils::GetGroupCount(VolumeParameters.TranslucencyGIGridSize, FMarkRadianceProbesUsedByTranslucencyVolumeCS::GetGroupSize());

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("MarkRadianceProbesUsedByTranslucencyVolume"),
		ComputePassFlags,
		ComputeShader,
		PassParameters,
		GroupSize);
}

void TraceVoxelsTranslucencyVolume(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View,
	bool bDynamicSkyLight,
	const FLumenCardTracingParameters& TracingParameters,
	LumenRadianceCache::FRadianceCacheInterpolationParameters RadianceCacheParameters,
	FLumenTranslucencyLightingVolumeParameters VolumeParameters,
	FLumenTranslucencyLightingVolumeTraceSetupParameters TraceSetupParameters,
	FRDGTextureRef VolumeTraceRadiance,
	FRDGTextureRef VolumeTraceHitDistance,
	FRDGTextureRef VolumeFroxelProbeRadianceHitDistance,
	ERDGPassFlags ComputePassFlags)
{
	FTranslucencyVolumeTraceVoxelsCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FTranslucencyVolumeTraceVoxelsCS::FParameters>();
	PassParameters->RWVolumeTraceRadiance = GraphBuilder.CreateUAV(VolumeTraceRadiance);
	PassParameters->RWVolumeTraceHitDistance = GraphBuilder.CreateUAV(VolumeTraceHitDistance);

	PassParameters->TracingParameters = TracingParameters;
	PassParameters->RadianceCacheParameters = RadianceCacheParameters;
	PassParameters->VolumeParameters = VolumeParameters;
	PassParameters->TraceSetupParameters = TraceSetupParameters;
	PassParameters->VolumeFroxelProbeRadianceHitDistance = VolumeFroxelProbeRadianceHitDistance;

	PassParameters->SceneTexturesStruct = View.GetSceneTextures().UniformBuffer;

	const bool bTraceFromVolume = CVarLumenTranslucencyVolumeTraceFromVolume.GetValueOnRenderThread() != 0;

	FTranslucencyVolumeTraceVoxelsCS::FPermutationDomain PermutationVector;
	PermutationVector.Set<FTranslucencyVolumeTraceVoxelsCS::FDynamicSkyLight>(bDynamicSkyLight);
	PermutationVector.Set<FTranslucencyVolumeTraceVoxelsCS::FProbeSourceMode>(VolumeFroxelProbeRadianceHitDistance != nullptr ? 2 : (RadianceCacheParameters.RadianceProbeIndirectionTexture != nullptr ? 1 : 0));
	PermutationVector.Set<FTranslucencyVolumeTraceVoxelsCS::FTraceFromVolume>(bTraceFromVolume);
	PermutationVector.Set<FTranslucencyVolumeTraceVoxelsCS::FSimpleCoverageBasedExpand>(bTraceFromVolume && Lumen::UseGlobalSDFSimpleCoverageBasedExpand());
	auto ComputeShader = View.ShaderMap->GetShader<FTranslucencyVolumeTraceVoxelsCS>(PermutationVector);

	const FIntVector GroupSize = FComputeShaderUtils::GetGroupCount(VolumeTraceRadiance->Desc.GetSize(), FTranslucencyVolumeTraceVoxelsCS::GetGroupSize());

	const int32 TranslucencyVolumeTracingOctahedronResolution= CVarTranslucencyVolumeTracingOctahedronResolution.GetValueOnRenderThread();
	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("%s %ux%u", bTraceFromVolume ? TEXT("TraceVoxels") : TEXT("RadianceCacheInterpolate"), TranslucencyVolumeTracingOctahedronResolution, TranslucencyVolumeTracingOctahedronResolution),
		ComputePassFlags,
		ComputeShader,
		PassParameters,
		GroupSize);
}

LumenRadianceCache::FUpdateInputs FDeferredShadingSceneRenderer::GetLumenTranslucencyGIVolumeRadianceCacheInputs(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View, 
	const FLumenSceneFrameTemporaries& FrameTemporaries,
	ERDGPassFlags ComputePassFlags)
{
	const FLumenTranslucencyLightingVolumeParameters VolumeParameters = GetTranslucencyLightingVolumeParameters(View);
	const LumenRadianceCache::FRadianceCacheInputs RadianceCacheInputs = LumenTranslucencyVolumeRadianceCache::SetupRadianceCacheInputs(View);

	FRadianceCacheConfiguration Configuration;
	Configuration.bFarField = CVarTranslucencyVolumeRadianceCacheFarField.GetValueOnRenderThread() != 0;

	FMarkUsedRadianceCacheProbes MarkUsedRadianceCacheProbesCallbacks;

	if (CVarLumenTranslucencyVolume.GetValueOnRenderThread() && CVarLumenTranslucencyVolumeRadianceCache.GetValueOnRenderThread()
		&& !GetVolumeRadianceCacheFrustumProbes()) // no need to request radiance cache if we use froxel probes.
	{
		MarkUsedRadianceCacheProbesCallbacks.AddLambda([VolumeParameters, ComputePassFlags](
			FRDGBuilder& GraphBuilder, 
			const FViewInfo& View, 
			const LumenRadianceCache::FRadianceCacheMarkParameters& RadianceCacheMarkParameters)
			{
				MarkRadianceProbesUsedByTranslucencyVolume(
					GraphBuilder,
					View,
					VolumeParameters,
					RadianceCacheMarkParameters,
					ComputePassFlags);
			});
	}

	LumenRadianceCache::FUpdateInputs RadianceCacheUpdateInputs(
		RadianceCacheInputs,
		Configuration,
		View,
		nullptr,
		nullptr,
		FMarkUsedRadianceCacheProbes(),
		MoveTemp(MarkUsedRadianceCacheProbesCallbacks));

	return RadianceCacheUpdateInputs;
}

void PrepareLumenHardwareRayTracingTranslucencyVolumeLumenMaterial2(const FViewInfo& View, TArray<FRHIRayTracingShader*>& OutRayGenShaders)
{
	if (Lumen::UseHardwareRayTracedTranslucencyVolume(*View.Family) && !Lumen::UseHardwareInlineRayTracing(*View.Family))
	{
#if RHI_RAYTRACING
		for (int32 DynamicSkyLight = 0; DynamicSkyLight < 2; ++DynamicSkyLight)
		{
			{
				FTranslucencyVolumeFroxelProbesHardwareRayTracingRGS::FPermutationDomain PermutationVector;
				PermutationVector.Set<FTranslucencyVolumeFroxelProbesHardwareRayTracingRGS::FDynamicSkyLight>(DynamicSkyLight > 0);
				TShaderRef<FTranslucencyVolumeFroxelProbesHardwareRayTracingRGS> RayGenerationShader = View.ShaderMap->GetShader<FTranslucencyVolumeFroxelProbesHardwareRayTracingRGS>(PermutationVector);

				OutRayGenShaders.Add(RayGenerationShader.GetRayTracingShader());
			}
		}
#endif
	}
}

void FDeferredShadingSceneRenderer::ComputeLumenTranslucencyGIVolume(
	FRDGBuilder& GraphBuilder,
	FViewInfo& View, 
	const FLumenSceneFrameTemporaries& FrameTemporaries,
	LumenRadianceCache::FRadianceCacheInterpolationParameters& RadianceCacheParameters,
	ERDGPassFlags ComputePassFlags)
{
	if (CVarLumenTranslucencyVolume.GetValueOnRenderThread())
	{
		RDG_EVENT_SCOPE(GraphBuilder, "TranslucencyVolumeLighting");

		const FMatrix44f UnjitteredPrevWorldToClip = FMatrix44f(View.PrevViewInfo.ViewMatrices.GetViewMatrix() * View.PrevViewInfo.ViewMatrices.ComputeProjectionNoAAMatrix());		// LWC_TODO: Precision loss?

		if (CVarLumenTranslucencyVolumeRadianceCache.GetValueOnRenderThread() && !RadianceCacheParameters.RadianceProbeIndirectionTexture)
		{
			LumenRadianceCache::TInlineArray<LumenRadianceCache::FUpdateInputs> InputArray;
			LumenRadianceCache::TInlineArray<LumenRadianceCache::FUpdateOutputs> OutputArray;

			LumenRadianceCache::FUpdateInputs TranslucencyVolumeRadianceCacheUpdateInputs = GetLumenTranslucencyGIVolumeRadianceCacheInputs(
				GraphBuilder,
				View, 
				FrameTemporaries,
				ComputePassFlags);

			if (TranslucencyVolumeRadianceCacheUpdateInputs.IsAnyCallbackBound())
			{
				InputArray.Add(TranslucencyVolumeRadianceCacheUpdateInputs);
				OutputArray.Add(LumenRadianceCache::FUpdateOutputs(
					View.ViewState->Lumen.TranslucencyVolumeRadianceCacheState,
					RadianceCacheParameters));

				LumenRadianceCache::UpdateRadianceCaches(
					GraphBuilder, 
					FrameTemporaries,
					InputArray,
					OutputArray,
					Scene,
					ViewFamily,
					LumenCardRenderer.bPropagateGlobalLightingChange,
					ComputePassFlags);
			}
		}

		{
			FLumenCardTracingParameters TracingParameters;
			GetLumenCardTracingParameters(GraphBuilder, View, *Scene->GetLumenSceneData(View), FrameTemporaries, /*bSurfaceCacheFeedback*/ false, TracingParameters);

			const FLumenTranslucencyLightingVolumeParameters VolumeParameters = GetTranslucencyLightingVolumeParameters(View);
			const FIntVector TranslucencyGridSize = VolumeParameters.TranslucencyGIGridSize;

			FLumenTranslucencyLightingVolumeTraceSetupParameters TraceSetupParameters;
			{
				TraceSetupParameters.StepFactor = FMath::Clamp(CVarTranslucencyVolumeTraceStepFactor.GetValueOnRenderThread(), .1f, 10.0f);
				TraceSetupParameters.MaxTraceDistance = Lumen::GetMaxTraceDistance(View);
				TraceSetupParameters.VoxelTraceStartDistanceScale = CVarTranslucencyVolumeVoxelTraceStartDistanceScale.GetValueOnRenderThread();
				TraceSetupParameters.MaxRayIntensity = CVarTranslucencyVolumeMaxRayIntensity.GetValueOnRenderThread();
			}

			const FIntVector OctahedralAtlasSize(
				TranslucencyGridSize.X * CVarTranslucencyVolumeTracingOctahedronResolution.GetValueOnRenderThread(),
				TranslucencyGridSize.Y * CVarTranslucencyVolumeTracingOctahedronResolution.GetValueOnRenderThread(),
				TranslucencyGridSize.Z);

			FRDGTextureDesc VolumeTraceRadianceDesc(FRDGTextureDesc::Create3D(OctahedralAtlasSize, PF_FloatRGB, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV));
			FRDGTextureDesc VolumeTraceHitDistanceDesc(FRDGTextureDesc::Create3D(OctahedralAtlasSize, PF_R16F, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV));
	
			FRDGTextureRef VolumeTraceRadiance = GraphBuilder.CreateTexture(VolumeTraceRadianceDesc, TEXT("Lumen.TranslucencyVolume.VolumeTraceRadiance"));
			FRDGTextureRef VolumeTraceHitDistance = GraphBuilder.CreateTexture(VolumeTraceHitDistanceDesc, TEXT("Lumen.TranslucencyVolume.VolumeTraceHitDistance"));

			FRDGTextureRef VolumeFroxelProbeRadianceHitDistance = nullptr;
			FRDGTextureRef VolumeFroxelLowResProbeRadiance = nullptr;
			FRDGTextureRef VolumeFroxelLowResProbeHitDistance = nullptr;
			if (GetVolumeRadianceCacheFrustumProbes())
			{
				// Cannot use PF_FloatRGB otherwise that can lead to temporal loss of energy as well as hue shift after reprojection.
				// Do reduce texture fetch we thus put the HitDistance in the alpha channel.
				EPixelFormat RadiancePixelFormat = PF_FloatRGBA;
				// Low resolution probes are not reprojected so they can use the potentially 111110 float format.
				EPixelFormat LowResRadiancePixelFormat = PF_FloatRGB;

				const FIntVector FroxelProbeAtlasSize(
					VolumeParameters.TranslucencyVolumeTracingFroxelProbesGridSize.X * VolumeParameters.TranslucencyVolumeTracingFroxelProbesOctahedronResolution,
					VolumeParameters.TranslucencyVolumeTracingFroxelProbesGridSize.Y * VolumeParameters.TranslucencyVolumeTracingFroxelProbesOctahedronResolution,
					VolumeParameters.TranslucencyVolumeTracingFroxelProbesGridSize.Z);
				FRDGTextureDesc VolumeFroxelProbeRadianceHitDistanceDesc(FRDGTextureDesc::Create3D(FroxelProbeAtlasSize, RadiancePixelFormat, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV));
				VolumeFroxelProbeRadianceHitDistance = GraphBuilder.CreateTexture(VolumeFroxelProbeRadianceHitDistanceDesc, TEXT("Lumen.TranslucencyVolume.FroxelProbeRadianceHitDistance"));

				const FIntVector FroxelProbeLowResAtlasSize(
					VolumeParameters.TranslucencyVolumeTracingFroxelProbesGridSize.X * VolumeParameters.TranslucencyVolumeTracingFroxelLowResProbesOctahedronResolution,
					VolumeParameters.TranslucencyVolumeTracingFroxelProbesGridSize.Y * VolumeParameters.TranslucencyVolumeTracingFroxelLowResProbesOctahedronResolution,
					VolumeParameters.TranslucencyVolumeTracingFroxelProbesGridSize.Z);
				FRDGTextureDesc VolumeFroxelLowResProbeRadianceDesc(FRDGTextureDesc::Create3D(FroxelProbeLowResAtlasSize, LowResRadiancePixelFormat, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV));
				FRDGTextureDesc VolumeFroxelLowResProbeHitDistanceDesc(FRDGTextureDesc::Create3D(FroxelProbeLowResAtlasSize, PF_R16F, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV));
				VolumeFroxelLowResProbeRadiance = GraphBuilder.CreateTexture(VolumeFroxelLowResProbeRadianceDesc, TEXT("Lumen.TranslucencyVolume.LowResFroxelProbeRadiance"));
				VolumeFroxelLowResProbeHitDistance = GraphBuilder.CreateTexture(VolumeFroxelLowResProbeHitDistanceDesc, TEXT("Lumen.TranslucencyVolume.LowResFroxelProbeHitDistance"));

				const bool bDynamicSkyLight = Lumen::ShouldHandleSkyLight(Scene, ViewFamily);

				bool bInlineRayTracing = Lumen::UseHardwareInlineRayTracing(*View.Family);

				{
					// Create buffer required for the froxel probe update scheduling
					const uint32 FroxelProbeCount = VolumeParameters.TranslucencyVolumeTracingFroxelProbesGridSize.X * VolumeParameters.TranslucencyVolumeTracingFroxelProbesGridSize.Y * VolumeParameters.TranslucencyVolumeTracingFroxelProbesGridSize.Z;
					const uint32 MaxFroxelProbeRefineRayTracedPerFrame = FroxelProbeCount * VolumeParameters.TranslucencyVolumeTracingFroxelProbeRefineTraceCountPerFrame;

					const uint32 R32_ByteSize			= sizeof(uint32) * 1;
					const uint32 R16G16B16A16_ByteSize	= sizeof(uint16) * 4;

					FRDGBufferRef		FroxelClearCountBuffer								= GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateByteAddressDesc(16), TEXT("Lumen.FroxelClearCountByteAddressBuffer"));
					FRDGBufferUAVRef	FroxelClearCountBufferUAV							= GraphBuilder.CreateUAV(FroxelClearCountBuffer, PF_R32_UINT);
					FRDGBufferSRVRef	FroxelClearCountBufferSRV							= GraphBuilder.CreateSRV(FroxelClearCountBuffer, PF_R32_UINT);

					FRDGBufferRef		FroxelClearListBuffer								= GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(R16G16B16A16_ByteSize, FroxelProbeCount), TEXT("Lumen.FroxelClearListBuffer"));
					FRDGBufferUAVRef	FroxelClearListBufferUAV							= GraphBuilder.CreateUAV(FroxelClearListBuffer, PF_R16G16B16A16_UINT);
					FRDGBufferSRVRef	FroxelClearListBufferSRV							= GraphBuilder.CreateSRV(FroxelClearListBuffer, PF_R16G16B16A16_UINT);

					FRDGBufferRef		FroxelClearDispatchIndirectBuffer					= GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateIndirectDesc<FRHIDispatchIndirectParameters>(1), TEXT("Lumen.FroxelClearDispatchIndirectBuffer"));
					FRDGBufferUAVRef	FroxelClearDispatchIndirectBufferUAV				= GraphBuilder.CreateUAV(FroxelClearDispatchIndirectBuffer, PF_R32_UINT);
					FRDGBufferSRVRef	FroxelClearDispatchIndirectBufferSRV				= GraphBuilder.CreateSRV(FroxelClearDispatchIndirectBuffer, PF_R32_UINT);
					
					// 8 bytes: 1uint for init and 1 uint for refine
					FRDGBufferRef		FroxelInitAndRefineCountBuffer						= GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateByteAddressDesc(16), TEXT("Lumen.FroxelInitAndRefineCountByteAddressBuffer"));
					FRDGBufferUAVRef	FroxelInitAndRefineCountBufferUAV					= GraphBuilder.CreateUAV(FroxelInitAndRefineCountBuffer, PF_R32_UINT);
					FRDGBufferSRVRef	FroxelInitAndRefineCountBufferSRV					= GraphBuilder.CreateSRV(FroxelInitAndRefineCountBuffer, PF_R32_UINT);

					// Contains the probe coordinate to initialise
					FRDGBufferRef		FroxelLowResInitListBuffer							= GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(R16G16B16A16_ByteSize, FroxelProbeCount), TEXT("Lumen.FroxelLowResInitListBuffer"));
					FRDGBufferUAVRef	FroxelLowResInitListBufferUAV						= GraphBuilder.CreateUAV(FroxelLowResInitListBuffer, PF_R16G16B16A16_UINT);
					FRDGBufferSRVRef	FroxelLowResInitListBufferSRV						= GraphBuilder.CreateSRV(FroxelLowResInitListBuffer, PF_R16G16B16A16_UINT);

					// Contains the probe coordinate + UV to init
					FRDGBufferRef		FroxelRefineListBuffer								= GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(R16G16B16A16_ByteSize, MaxFroxelProbeRefineRayTracedPerFrame), TEXT("Lumen.FroxelRefineListBuffer"));
					FRDGBufferUAVRef	FroxelRefineListBufferUAV							= GraphBuilder.CreateUAV(FroxelRefineListBuffer, PF_R16G16B16A16_UINT);
					FRDGBufferSRVRef	FroxelRefineListBufferSRV							= GraphBuilder.CreateSRV(FroxelRefineListBuffer, PF_R16G16B16A16_UINT);

					FRDGBufferRef		FroxelInitAndRefineTraceDispatchIndirectBuffer		= GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateIndirectDesc<FRHIDispatchIndirectParameters>(1), TEXT("Lumen.FroxelInitAndRefineTraceDispatchIndirectBuffer"));
					FRDGBufferUAVRef	FroxelInitAndRefineTraceDispatchIndirectBufferUAV	= GraphBuilder.CreateUAV(FroxelInitAndRefineTraceDispatchIndirectBuffer, PF_R32_UINT);
					FRDGBufferSRVRef	FroxelInitAndRefineTraceDispatchIndirectBufferSRV	= GraphBuilder.CreateSRV(FroxelInitAndRefineTraceDispatchIndirectBuffer, PF_R32_UINT);

					FRDGBufferRef		FroxelLowResCopyDispatchIndirectBuffer				= GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateIndirectDesc<FRHIDispatchIndirectParameters>(1), TEXT("Lumen.FroxelLowResCopyDispatchIndirectBuffer"));
					FRDGBufferUAVRef	FroxelLowResCopyDispatchIndirectBufferUAV			= GraphBuilder.CreateUAV(FroxelLowResCopyDispatchIndirectBuffer, PF_R32_UINT);
					FRDGBufferSRVRef	FroxelLowResCopyDispatchIndirectBufferSRV			= GraphBuilder.CreateSRV(FroxelLowResCopyDispatchIndirectBuffer, PF_R32_UINT);

					FRDGBufferRef		FroxelReprojRefineDispatchIndirectBuffer			= GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateIndirectDesc<FRHIDispatchIndirectParameters>(1), TEXT("Lumen.FroxelReprojRefineDispatchIndirectBuffer"));
					FRDGBufferUAVRef	FroxelReprojRefineDispatchIndirectBufferUAV			= GraphBuilder.CreateUAV(FroxelReprojRefineDispatchIndirectBuffer, PF_R32_UINT);
					FRDGBufferSRVRef	FroxelReprojRefineDispatchIndirectBufferSRV			= GraphBuilder.CreateSRV(FroxelReprojRefineDispatchIndirectBuffer, PF_R32_UINT);

					// Clear buffers that need to be cleared
					AddClearUAVPass(GraphBuilder, FroxelClearCountBufferUAV, 0);
					AddClearUAVPass(GraphBuilder, FroxelInitAndRefineCountBufferUAV, 0);

					////////////////////////////////////////
					// Generate the samples we need this frame using LFSR to make sure we cover the full set of pixels in a minimum amount of frames (true for power of two)
					////////////////////////////////////////
					FUintVector4 ProbeSamplesToTrace[MAX_FRUSTUM_PROBES_REFINE_TRACEPERFRAME];
					if (View.ViewState)
					{
						const uint32 PowerOfTwoProbeOctahedronRes = FMath::RoundUpToPowerOfTwo(VolumeParameters.TranslucencyVolumeTracingFroxelProbesOctahedronResolution);
						const uint32 LFSRTexelCount = PowerOfTwoProbeOctahedronRes * PowerOfTwoProbeOctahedronRes;
						for (uint32 i = 0; i < VolumeParameters.TranslucencyVolumeTracingFroxelProbeRefineTraceCountPerFrame; ++i)
						{
							uint32 Bitcount = 0;
							uint32 SquareResolution = 0;
							if (LFSRTexelCount == 4)
							{
								Bitcount = 2;
								SquareResolution = 2;
							}
							else if (LFSRTexelCount == 16)
							{
								Bitcount = 4;
								SquareResolution = 4;
							}
							else if (LFSRTexelCount == 64)
							{
								Bitcount = 6;
								SquareResolution = 8;
							}
							else if (LFSRTexelCount == 256)
							{
								Bitcount = 8;
								SquareResolution = 16;
							}
							else if (LFSRTexelCount == 1024)
							{
								Bitcount = 10;
								SquareResolution = 32;
							}
							else if (LFSRTexelCount == 4096)
							{
								Bitcount = 12;
								SquareResolution = 64;
							}
							else
							{
								check(false);	// this should not happen given a square with size of power of two.
							}
							uint32 NewValue = View.ViewState->Lumen.FroxelProbesLFSR.GetNextValueWithLast(Bitcount);

							uint32 CoordX = NewValue / SquareResolution;
							uint32 CoordY = NewValue - CoordX * SquareResolution;

							// Since the RoundUpToPowerOfTwo the probe resolution, the square of pixel we parse can be larger than the actual probe resolution, that is why we modulate by the ProbesOctahedronResolution.
							// This means that some texel will update at a high rate for some probe resolution.
							ProbeSamplesToTrace[i] = FUintVector4(CoordX % VolumeParameters.TranslucencyVolumeTracingFroxelProbesOctahedronResolution, CoordY % VolumeParameters.TranslucencyVolumeTracingFroxelProbesOctahedronResolution, 0, 0);
						}
					}
					else
					{
						// Default pixel update when no state is available, which should not happen anyway.
						static uint32 FallBackTracing = 0;
						for (uint32 i = 0; i < VolumeParameters.TranslucencyVolumeTracingFroxelProbeRefineTraceCountPerFrame; ++i)
						{
							// Halton takes more time to update all pixels of a 16x16 probe.so LFSR is preferred when there is a state.
							uint32 CoordX = Halton(View.ViewState->FrameIndex * VolumeParameters.TranslucencyVolumeTracingFroxelProbeRefineTraceCountPerFrame + i, 2) * VolumeParameters.TranslucencyVolumeTracingFroxelProbesOctahedronResolution;
							uint32 CoordY = Halton(View.ViewState->FrameIndex * VolumeParameters.TranslucencyVolumeTracingFroxelProbeRefineTraceCountPerFrame + i, 3) * VolumeParameters.TranslucencyVolumeTracingFroxelProbesOctahedronResolution;
							ProbeSamplesToTrace[i] = FUintVector4(CoordX % VolumeParameters.TranslucencyVolumeTracingFroxelProbesOctahedronResolution, CoordY % VolumeParameters.TranslucencyVolumeTracingFroxelProbesOctahedronResolution, 0, 0);
							FallBackTracing++;
						}
					}

					float ViewFroxelProbesHistoryPreExposure = 1.0f;
					FRDGTextureRef ViewVolumeFroxelProbeRadianceHitDistanceHistory = GSystemTextures.GetVolumetricBlackDummy(GraphBuilder);
					if (View.ViewState && View.ViewState->Lumen.ViewVolumeFroxelProbeRadianceHitDistance.IsValid())
					{
						ViewVolumeFroxelProbeRadianceHitDistanceHistory = GraphBuilder.RegisterExternalTexture(View.ViewState->Lumen.ViewVolumeFroxelProbeRadianceHitDistance);
						ViewFroxelProbesHistoryPreExposure = View.ViewState->Lumen.ViewFroxelProbesHistoryPreExposure;
					}
					const FVector2f ViewFroxelProbesHistoryPreExposureAndInv = FVector2f(ViewFroxelProbesHistoryPreExposure, ViewFroxelProbesHistoryPreExposure > 0.0f ? 1.0f / ViewFroxelProbesHistoryPreExposure : 1.0f);

					const bool bCameraCut = 
						!( View.ViewState
						&& !View.bCameraCut
						&& !View.bPrevTransformsReset
						&& ViewFamily.bRealtimeUpdate
						&& ViewVolumeFroxelProbeRadianceHitDistanceHistory
						&& ViewVolumeFroxelProbeRadianceHitDistanceHistory->Desc == VolumeFroxelProbeRadianceHitDistanceDesc);

					////////////////////////////////////////
					// Schedule froxel update
					////////////////////////////////////////
					{
						FFroxelProbesUpdateSchedulerCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FFroxelProbesUpdateSchedulerCS::FParameters>();
						PassParameters->View = View.ViewUniformBuffer;
						PassParameters->VolumeParameters = VolumeParameters;
						PassParameters->FroxelClearCountBufferUAV = FroxelClearCountBufferUAV;
						PassParameters->FroxelClearListBufferUAV = FroxelClearListBufferUAV;
						PassParameters->FroxelInitAndRefineCountBufferUAV = FroxelInitAndRefineCountBufferUAV;
						PassParameters->FroxelLowResInitListBufferUAV = FroxelLowResInitListBufferUAV;
						PassParameters->FroxelRefineListBufferUAV = FroxelRefineListBufferUAV;
						PassParameters->UnjitteredPrevWorldToClip = UnjitteredPrevWorldToClip;
						PassParameters->CameraCut = bCameraCut ? 1 : 0;

						for (uint32 i = 0; i < VolumeParameters.TranslucencyVolumeTracingFroxelProbeRefineTraceCountPerFrame; ++i)
						{
							PassParameters->ProbeSamplesToTrace[i] = ProbeSamplesToTrace[i];
						}

						FFroxelProbesUpdateSchedulerCS::FPermutationDomain PermutationVector;
						auto ComputeShader = View.ShaderMap->GetShader<FFroxelProbesUpdateSchedulerCS>(PermutationVector);

						const FIntVector GroupSize = FComputeShaderUtils::GetGroupCount(FIntVector(VolumeParameters.TranslucencyVolumeTracingFroxelProbesGridSize), FFroxelProbesUpdateSchedulerCS::GetGroupSize());

						FComputeShaderUtils::AddPass(
							GraphBuilder,
							RDG_EVENT_NAME("%s %ux%ux%u", TEXT("FroxelProbesUpdateScheduler"), VolumeParameters.TranslucencyVolumeTracingFroxelProbesGridSize.X, VolumeParameters.TranslucencyVolumeTracingFroxelProbesGridSize.Y, VolumeParameters.TranslucencyVolumeTracingFroxelProbesGridSize.Z),
							ComputePassFlags,
							ComputeShader,
							PassParameters,
							GroupSize);
					}

					////////////////////////////////////////
					// Create indirect dispatch buffers except ray tracing one
					////////////////////////////////////////
					{
						FFroxelProbesUpdateIndirectArgsSetupCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FFroxelProbesUpdateIndirectArgsSetupCS::FParameters>();
						PassParameters->VolumeParameters = VolumeParameters;
						PassParameters->FroxelClearCountBuffer = FroxelClearCountBufferSRV;
						PassParameters->FroxelClearDispatchIndirectBufferUAV = FroxelClearDispatchIndirectBufferUAV;
						PassParameters->FroxelClearCountBuffer = FroxelClearCountBufferSRV;
						PassParameters->FroxelInitAndRefineCountBuffer = FroxelInitAndRefineCountBufferSRV;
						PassParameters->FroxelInitAndRefineTraceDispatchIndirectBufferUAV = FroxelInitAndRefineTraceDispatchIndirectBufferUAV;
						PassParameters->FroxelLowResCopyDispatchIndirectBufferUAV = FroxelLowResCopyDispatchIndirectBufferUAV;
						PassParameters->FroxelReprojRefineDispatchIndirectBufferUAV = FroxelReprojRefineDispatchIndirectBufferUAV;
					#if RHI_RAYTRACING
						PassParameters->RayTracingThreadGroupSize = bInlineRayTracing ? FTranslucencyVolumeFroxelProbesHardwareRayTracingCS::GetThreadGroupSize(View.GetShaderPlatform()) : FTranslucencyVolumeFroxelProbesHardwareRayTracingRGS::GetThreadGroupSize();
					#else
						PassParameters->RayTracingThreadGroupSize = FIntPoint(0, 0);
					#endif
						PassParameters->bUsingRayTracing = Lumen::UseHardwareRayTracedTranslucencyVolume(ViewFamily) ? 1 : 0;

						const bool bDebugFroxelProbesUpdateIndirectArgs = CVarTranslucencyVolumeRadianceCacheFrustumProbesDebug.GetValueOnRenderThread() > 0;
						if (bDebugFroxelProbesUpdateIndirectArgs)
						{
							ShaderPrint::SetEnabled(true);
							ShaderPrint::RequestSpaceForCharacters(256);
							ShaderPrint::SetParameters(GraphBuilder, View.ShaderPrintData, PassParameters->ShaderPrintUniformBuffer);
						}

						FFroxelProbesUpdateIndirectArgsSetupCS::FPermutationDomain PermutationVector;
						PermutationVector.Set<FFroxelProbesUpdateIndirectArgsSetupCS::FDebugPrint>(bDebugFroxelProbesUpdateIndirectArgs);
						auto ComputeShader = View.ShaderMap->GetShader<FFroxelProbesUpdateIndirectArgsSetupCS>(PermutationVector);

						const FIntVector GroupSize = FIntVector(1, 1, 1);

						FComputeShaderUtils::AddPass(
							GraphBuilder,
							RDG_EVENT_NAME("FroxelProbesUpdateIndirectArgsSetup"),
							ComputePassFlags,
							ComputeShader,
							PassParameters,
							GroupSize);
					}

					////////////////////////////////////////
					// Trace all the rays at once and write directly where it is needed.
					////////////////////////////////////////
					{
					#if RHI_RAYTRACING
						if (Lumen::UseHardwareRayTracedTranslucencyVolume(ViewFamily))
						{

							FTranslucencyVolumeFroxelProbesHardwareRayTracing::FParameters* PassParameters = GraphBuilder.AllocParameters<FTranslucencyVolumeFroxelProbesHardwareRayTracing::FParameters>();

							PassParameters->RWVolumeTraceRadiance    = GraphBuilder.CreateUAV(VolumeFroxelLowResProbeRadiance);
							PassParameters->RWVolumeTraceHitDistance = GraphBuilder.CreateUAV(VolumeFroxelLowResProbeHitDistance);
							
							SetLumenHardwareRayTracingSharedParameters(
								GraphBuilder,
								GetSceneTextureParameters(GraphBuilder, View),
								View,
								TracingParameters,
								&PassParameters->SharedParameters);
							
							PassParameters->RadianceCacheParameters = RadianceCacheParameters;
							PassParameters->VolumeParameters = VolumeParameters;
							PassParameters->TraceSetupParameters = TraceSetupParameters;
							
							// Indirect fill up data
							PassParameters->FroxelProbeRayTraceDispatchIndirectBuffer = FroxelInitAndRefineTraceDispatchIndirectBuffer;
							PassParameters->FroxelInitAndRefineCountBuffer = FroxelInitAndRefineCountBufferSRV;
							PassParameters->FroxelLowResInitListBuffer = FroxelLowResInitListBufferSRV;
							PassParameters->FroxelRefineListBufferSRV = FroxelRefineListBufferSRV;

							FTranslucencyVolumeFroxelProbesHardwareRayTracingRGS::FPermutationDomain PermutationVector;
							PermutationVector.Set<FTranslucencyVolumeFroxelProbesHardwareRayTracingRGS::FDynamicSkyLight>(bDynamicSkyLight);

							if (bInlineRayTracing)
							{
								FTranslucencyVolumeFroxelProbesHardwareRayTracingCS::AddLumenRayTracingDispatchIndirect(
									GraphBuilder,
									RDG_EVENT_NAME("ProbesHardwareRayTracing (inline CS)"),
									View,
									PermutationVector,
									PassParameters,
									PassParameters->FroxelProbeRayTraceDispatchIndirectBuffer,
									0, // IndirectArgsOffset
									ComputePassFlags);
							}
							else
							{
								const bool bUseMinimalPayload = true;
								FTranslucencyVolumeFroxelProbesHardwareRayTracingRGS::AddLumenRayTracingDispatchIndirect(
									GraphBuilder,
									RDG_EVENT_NAME("ProbesHardwareRayTracing"),
									View,
									PermutationVector,
									PassParameters,
									PassParameters->FroxelProbeRayTraceDispatchIndirectBuffer,
									0, // IndirectArgsOffset
									bUseMinimalPayload);
							}
						}
						else
					#endif // RHI_RAYTRACING
						{
							FTranslucencyVolumeFroxelProbesSoftwareRayTracingCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FTranslucencyVolumeFroxelProbesSoftwareRayTracingCS::FParameters>();
							PassParameters->RWVolumeTraceRadiance = GraphBuilder.CreateUAV(VolumeFroxelLowResProbeRadiance);
							PassParameters->RWVolumeTraceHitDistance = GraphBuilder.CreateUAV(VolumeFroxelLowResProbeHitDistance);

							PassParameters->TracingParameters = TracingParameters;
							PassParameters->RadianceCacheParameters = RadianceCacheParameters;
							PassParameters->VolumeParameters = VolumeParameters;
							PassParameters->TraceSetupParameters = TraceSetupParameters;

							// Indirect fill up data
							PassParameters->FroxelProbeRayTraceDispatchIndirectBuffer = FroxelInitAndRefineTraceDispatchIndirectBuffer;
							PassParameters->FroxelInitAndRefineCountBuffer = FroxelInitAndRefineCountBufferSRV;
							PassParameters->FroxelLowResInitListBuffer = FroxelLowResInitListBufferSRV;
							PassParameters->FroxelRefineListBufferSRV = FroxelRefineListBufferSRV;

							PassParameters->SceneTexturesStruct = View.GetSceneTextures().UniformBuffer;

							FTranslucencyVolumeFroxelProbesSoftwareRayTracingCS::FPermutationDomain PermutationVector;
							PermutationVector.Set<FTranslucencyVolumeFroxelProbesSoftwareRayTracingCS::FDynamicSkyLight>(bDynamicSkyLight);
							PermutationVector.Set<FTranslucencyVolumeFroxelProbesSoftwareRayTracingCS::FSimpleCoverageBasedExpand>(Lumen::UseGlobalSDFSimpleCoverageBasedExpand());
							auto ComputeShader = View.ShaderMap->GetShader<FTranslucencyVolumeFroxelProbesSoftwareRayTracingCS>(PermutationVector);

							ClearUnusedGraphResources(ComputeShader, PassParameters);
							GraphBuilder.AddPass(
								RDG_EVENT_NAME("ProbesSoftwareRayTracingCS"),
								PassParameters,
								ERDGPassFlags::Compute,
								[PassParameters, ComputeShader](FRHICommandList& RHICmdList)
								{
									FComputeShaderUtils::DispatchIndirect(RHICmdList, ComputeShader, *PassParameters, PassParameters->FroxelProbeRayTraceDispatchIndirectBuffer->GetIndirectRHICallBuffer(), 0);
								});
						}
					}

					////////////////////////////////////////
					// Clear froxel that needs to be.
					////////////////////////////////////////
					{
						FUpdateFroxelProbesCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FUpdateFroxelProbesCS::FParameters>();
						PassParameters->View = View.ViewUniformBuffer;
						PassParameters->VolumeFroxelProbeRadianceHitDistanceHistory = nullptr;
						PassParameters->VolumeFroxelLowResProbeRadiance = nullptr;
						PassParameters->VolumeFroxelLowResProbeHitDistance = nullptr;
						PassParameters->RWVolumeFroxelProbeRadianceHitDistance = GraphBuilder.CreateUAV(VolumeFroxelProbeRadianceHitDistance);

						PassParameters->VolumeParameters = VolumeParameters;
						PassParameters->UnjitteredPrevWorldToClip = UnjitteredPrevWorldToClip;
						PassParameters->ViewFroxelProbesHistoryPreExposureAndInv = ViewFroxelProbesHistoryPreExposureAndInv;

						// Indirect fill up data
						PassParameters->DispatchIndirectBuffer = FroxelClearDispatchIndirectBuffer;
						PassParameters->FroxelClearCountBuffer = FroxelClearCountBufferSRV;
						PassParameters->FroxelInitAndRefineCountBuffer = nullptr;
						PassParameters->FroxelUpdateListBuffer = FroxelClearListBufferSRV;

						FUpdateFroxelProbesCS::FPermutationDomain PermutationVector;
						PermutationVector.Set<FUpdateFroxelProbesCS::FProbeFillMode>(2); // Clear mode
						auto ComputeShader = View.ShaderMap->GetShader<FUpdateFroxelProbesCS>(PermutationVector);

						ClearUnusedGraphResources(ComputeShader, PassParameters);
						GraphBuilder.AddPass(
							RDG_EVENT_NAME("FroxelProbesClear"),
							PassParameters,
							ERDGPassFlags::Compute,
							[PassParameters, ComputeShader](FRHICommandList& RHICmdList)
							{
								FComputeShaderUtils::DispatchIndirect(RHICmdList, ComputeShader, *PassParameters, PassParameters->DispatchIndirectBuffer->GetIndirectRHICallBuffer(), 0);
							});
					}

					////////////////////////////////////////
					// Copy low res probe onto froxels that needs to be initialised.
					////////////////////////////////////////
					{
						FUpdateFroxelProbesCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FUpdateFroxelProbesCS::FParameters>();
						PassParameters->View = View.ViewUniformBuffer;
						PassParameters->VolumeFroxelProbeRadianceHitDistanceHistory = nullptr;
						PassParameters->VolumeFroxelLowResProbeRadiance = VolumeFroxelLowResProbeRadiance;
						PassParameters->VolumeFroxelLowResProbeHitDistance = VolumeFroxelLowResProbeHitDistance;
						PassParameters->RWVolumeFroxelProbeRadianceHitDistance = GraphBuilder.CreateUAV(VolumeFroxelProbeRadianceHitDistance);
						
						PassParameters->VolumeParameters = VolumeParameters;
						PassParameters->UnjitteredPrevWorldToClip = UnjitteredPrevWorldToClip;
						PassParameters->ViewFroxelProbesHistoryPreExposureAndInv = ViewFroxelProbesHistoryPreExposureAndInv;
						
						// Indirect fill up data
						PassParameters->DispatchIndirectBuffer = FroxelLowResCopyDispatchIndirectBuffer;
						PassParameters->FroxelClearCountBuffer = nullptr;
						PassParameters->FroxelInitAndRefineCountBuffer = FroxelInitAndRefineCountBufferSRV;
						PassParameters->FroxelUpdateListBuffer = FroxelLowResInitListBufferSRV;
						
						FUpdateFroxelProbesCS::FPermutationDomain PermutationVector;
						PermutationVector.Set<FUpdateFroxelProbesCS::FProbeFillMode>(1); // Reset with low res probes
						auto ComputeShader = View.ShaderMap->GetShader<FUpdateFroxelProbesCS>(PermutationVector);
						
						ClearUnusedGraphResources(ComputeShader, PassParameters);
						GraphBuilder.AddPass(
							RDG_EVENT_NAME("FroxelProbesCopyLowResInit"),
							PassParameters,
							ERDGPassFlags::Compute,
							[PassParameters, ComputeShader](FRHICommandList& RHICmdList)
							{
								FComputeShaderUtils::DispatchIndirect(RHICmdList, ComputeShader, *PassParameters, PassParameters->DispatchIndirectBuffer->GetIndirectRHICallBuffer(), 0);
							});
					}

					////////////////////////////////////////
					// Reproject probes that can be from last frame and refine with extra rays.
					////////////////////////////////////////
					{
						FUpdateFroxelProbesCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FUpdateFroxelProbesCS::FParameters>();
						PassParameters->View = View.ViewUniformBuffer;
						PassParameters->VolumeFroxelProbeRadianceHitDistanceHistory = ViewVolumeFroxelProbeRadianceHitDistanceHistory;
						PassParameters->VolumeFroxelLowResProbeRadiance = VolumeFroxelLowResProbeRadiance;
						PassParameters->VolumeFroxelLowResProbeHitDistance = VolumeFroxelLowResProbeHitDistance;
						PassParameters->RWVolumeFroxelProbeRadianceHitDistance = GraphBuilder.CreateUAV(VolumeFroxelProbeRadianceHitDistance);

						PassParameters->VolumeParameters = VolumeParameters;
						PassParameters->UnjitteredPrevWorldToClip = UnjitteredPrevWorldToClip;
						PassParameters->ViewFroxelProbesHistoryPreExposureAndInv = ViewFroxelProbesHistoryPreExposureAndInv;

						// Indirect fill up data
						PassParameters->DispatchIndirectBuffer = FroxelReprojRefineDispatchIndirectBuffer;
						PassParameters->FroxelClearCountBuffer = nullptr;
						PassParameters->FroxelInitAndRefineCountBuffer = FroxelInitAndRefineCountBufferSRV;
						PassParameters->FroxelUpdateListBuffer = FroxelRefineListBufferSRV;

						for (uint32 i = 0; i < VolumeParameters.TranslucencyVolumeTracingFroxelProbeRefineTraceCountPerFrame; ++i)
						{
							PassParameters->ProbeSamplesToTrace[i] = ProbeSamplesToTrace[i];
						}

						FUpdateFroxelProbesCS::FPermutationDomain PermutationVector;
						PermutationVector.Set<FUpdateFroxelProbesCS::FProbeFillMode>(0); // Reproject and refine probes
						auto ComputeShader = View.ShaderMap->GetShader<FUpdateFroxelProbesCS>(PermutationVector);

						ClearUnusedGraphResources(ComputeShader, PassParameters);
						GraphBuilder.AddPass(
							RDG_EVENT_NAME("FroxelProbesReprojectRefine"),
							PassParameters,
							ERDGPassFlags::Compute,
							[PassParameters, ComputeShader](FRHICommandList& RHICmdList)
							{
								FComputeShaderUtils::DispatchIndirect(RHICmdList, ComputeShader, *PassParameters, PassParameters->DispatchIndirectBuffer->GetIndirectRHICallBuffer(), 0);
							});
					}

					if (View.ViewState && !View.bStatePrevViewInfoIsReadOnly)
					{
						View.ViewState->Lumen.ViewVolumeFroxelProbeRadianceHitDistance = GraphBuilder.ConvertToExternalTexture(VolumeFroxelProbeRadianceHitDistance);

						View.ViewState->Lumen.ViewFroxelProbesHistoryPreExposure = ViewFroxelProbesHistoryPreExposure;
					}
				}
			}

			if (Lumen::UseHardwareRayTracedTranslucencyVolume(ViewFamily) && CVarLumenTranslucencyVolumeTraceFromVolume.GetValueOnRenderThread() != 0)
			{
				HardwareRayTraceTranslucencyVolume(
					GraphBuilder,
					View,
					TracingParameters,
					RadianceCacheParameters,
					VolumeParameters,
					TraceSetupParameters, 
					VolumeTraceRadiance, 
					VolumeTraceHitDistance,
					VolumeFroxelProbeRadianceHitDistance,
					ComputePassFlags);
			}
			else
			{
				const bool bDynamicSkyLight = Lumen::ShouldHandleSkyLight(Scene, ViewFamily);
				TraceVoxelsTranslucencyVolume(
					GraphBuilder,
					View,
					bDynamicSkyLight,
					TracingParameters,
					RadianceCacheParameters,
					VolumeParameters,
					TraceSetupParameters,
					VolumeTraceRadiance,
					VolumeTraceHitDistance,
					VolumeFroxelProbeRadianceHitDistance,
					ComputePassFlags);
			}

			if (CVarTranslucencyVolumeSpatialFilter.GetValueOnRenderThread())
			{
				for (int32 PassIndex = 0; PassIndex < 3; PassIndex++) // 3 passes for the separable filter , one for each axis
				{
					FRDGTextureRef FilteredVolumeTraceRadiance = GraphBuilder.CreateTexture(VolumeTraceRadianceDesc, TEXT("Lumen.TranslucencyVolume.FilteredVolumeTraceRadiance"));

					FTranslucencyVolumeSpatialSeparableFilterCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FTranslucencyVolumeSpatialSeparableFilterCS::FParameters>();
					PassParameters->RWVolumeTraceRadiance = GraphBuilder.CreateUAV(FilteredVolumeTraceRadiance);

					PassParameters->VolumeTraceRadiance = VolumeTraceRadiance;
					PassParameters->VolumeTraceHitDistance = VolumeTraceHitDistance;
					PassParameters->View = View.ViewUniformBuffer;
					PassParameters->VolumeParameters = VolumeParameters;

					const int32 PreviousFrameIndexOffset = View.bStatePrevViewInfoIsReadOnly ? 0 : 1;
					PassParameters->PreviousFrameJitterOffset = (FVector3f)TranslucencyVolumeTemporalRandom(View.ViewState ? View.ViewState->GetFrameIndex() - PreviousFrameIndexOffset : 0);
					PassParameters->UnjitteredPrevWorldToClip = UnjitteredPrevWorldToClip;

					PassParameters->SpatialFilterDirection = FIntVector3(PassIndex == 0 ? 1 : 0, PassIndex == 1 ? 1 : 0, PassIndex == 2 ? 1 : 0);
					PassParameters->SpatialFilterSampleCount = FMath::Max(1, CVarTranslucencyVolumeSpatialFilterSampleCount.GetValueOnRenderThread());

					const float GaussianFilterStandardDev = FMath::Max(0.1, CVarTranslucencyVolumeSpatialFilterStandardDeviation.GetValueOnRenderThread());
					PassParameters->SpatialFilterGaussParams = FVector3f(GaussianFilterStandardDev, 1.0f/(2.0f*GaussianFilterStandardDev*GaussianFilterStandardDev), 1.0/(GaussianFilterStandardDev*FMath::Sqrt(2.0f*PI)));

					FTranslucencyVolumeSpatialSeparableFilterCS::FPermutationDomain PermutationVector;
					auto ComputeShader = View.ShaderMap->GetShader<FTranslucencyVolumeSpatialSeparableFilterCS>(PermutationVector);

					const FIntVector GroupSize = FComputeShaderUtils::GetGroupCount(OctahedralAtlasSize, FTranslucencyVolumeSpatialSeparableFilterCS::GetGroupSize());

					FComputeShaderUtils::AddPass(
						GraphBuilder,
						RDG_EVENT_NAME("SpatialFilter"),
						ComputePassFlags,
						ComputeShader,
						PassParameters,
						GroupSize);

					VolumeTraceRadiance = FilteredVolumeTraceRadiance;
				}
			}

			FRDGTextureRef TranslucencyGIVolumeHistory0 = nullptr;
			FRDGTextureRef TranslucencyGIVolumeHistory1 = nullptr;

			if (View.ViewState && View.ViewState->Lumen.TranslucencyVolume0)
			{
				TranslucencyGIVolumeHistory0 = GraphBuilder.RegisterExternalTexture(View.ViewState->Lumen.TranslucencyVolume0);
				TranslucencyGIVolumeHistory1 = GraphBuilder.RegisterExternalTexture(View.ViewState->Lumen.TranslucencyVolume1);
			}

			FRDGTextureDesc LumenTranslucencyGIDesc0(FRDGTextureDesc::Create3D(TranslucencyGridSize, PF_FloatRGB, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV | TexCreate_3DTiling));
			FRDGTextureDesc LumenTranslucencyGIDesc1(FRDGTextureDesc::Create3D(TranslucencyGridSize, PF_FloatRGBA, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV | TexCreate_3DTiling));
	
			FRDGTextureRef TranslucencyGIVolume0 = GraphBuilder.CreateTexture(LumenTranslucencyGIDesc0, TEXT("Lumen.TranslucencyVolume.SHLighting0"));
			FRDGTextureRef TranslucencyGIVolume1 = GraphBuilder.CreateTexture(LumenTranslucencyGIDesc1, TEXT("Lumen.TranslucencyVolume.SHLighting1"));
			FRDGTextureUAVRef TranslucencyGIVolume0UAV = GraphBuilder.CreateUAV(TranslucencyGIVolume0);
			FRDGTextureUAVRef TranslucencyGIVolume1UAV = GraphBuilder.CreateUAV(TranslucencyGIVolume1);

			FRDGTextureRef TranslucencyGIVolumeNewHistory0 = GraphBuilder.CreateTexture(LumenTranslucencyGIDesc0, TEXT("Lumen.TranslucencyVolume.SHLightingNewHistory0"));
			FRDGTextureRef TranslucencyGIVolumeNewHistory1 = GraphBuilder.CreateTexture(LumenTranslucencyGIDesc1, TEXT("Lumen.TranslucencyVolume.SHLightingNewHistory0"));
			FRDGTextureUAVRef TranslucencyGIVolumeNewHistory0UAV = GraphBuilder.CreateUAV(TranslucencyGIVolumeNewHistory0);
			FRDGTextureUAVRef TranslucencyGIVolumeNewHistory1UAV = GraphBuilder.CreateUAV(TranslucencyGIVolumeNewHistory1);

			{
				FTranslucencyVolumeIntegrateCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FTranslucencyVolumeIntegrateCS::FParameters>();
				PassParameters->RWTranslucencyGI0 = TranslucencyGIVolume0UAV;
				PassParameters->RWTranslucencyGI1 = TranslucencyGIVolume1UAV;
				PassParameters->RWTranslucencyGINewHistory0 = TranslucencyGIVolumeNewHistory0UAV;
				PassParameters->RWTranslucencyGINewHistory1 = TranslucencyGIVolumeNewHistory1UAV;

				PassParameters->View = View.ViewUniformBuffer;
				PassParameters->VolumeTraceRadiance = VolumeTraceRadiance;
				PassParameters->VolumeTraceHitDistance = VolumeTraceHitDistance;
				PassParameters->VolumeParameters = VolumeParameters;

				const bool bUseTemporalReprojection =
					CVarTranslucencyVolumeTemporalReprojection.GetValueOnRenderThread()
					&& View.ViewState
					&& !View.bCameraCut
					&& !View.bPrevTransformsReset
					&& ViewFamily.bRealtimeUpdate
					&& TranslucencyGIVolumeHistory0
					&& TranslucencyGIVolumeHistory0->Desc == LumenTranslucencyGIDesc0;

				PassParameters->HistoryWeight = CVarTranslucencyVolumeHistoryWeight.GetValueOnRenderThread();
				const int32 PreviousFrameIndexOffset = View.bStatePrevViewInfoIsReadOnly ? 0 : 1;
				PassParameters->PreviousFrameJitterOffset = (FVector3f)TranslucencyVolumeTemporalRandom(View.ViewState ? View.ViewState->GetFrameIndex() - PreviousFrameIndexOffset : 0);
				PassParameters->UnjitteredPrevWorldToClip = UnjitteredPrevWorldToClip;
				PassParameters->TranslucencyGIHistory0 = TranslucencyGIVolumeHistory0;
				PassParameters->TranslucencyGIHistory1 = TranslucencyGIVolumeHistory1;
				PassParameters->TranslucencyGIHistorySampler = TStaticSamplerState<SF_Trilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

				FTranslucencyVolumeIntegrateCS::FPermutationDomain PermutationVector;
				PermutationVector.Set<FTranslucencyVolumeIntegrateCS::FTemporalReprojection>(bUseTemporalReprojection);
				auto ComputeShader = View.ShaderMap->GetShader<FTranslucencyVolumeIntegrateCS>(PermutationVector);

				const FIntVector GroupSize = FComputeShaderUtils::GetGroupCount(TranslucencyGridSize, FTranslucencyVolumeIntegrateCS::GetGroupSize());

				FComputeShaderUtils::AddPass(
					GraphBuilder,
					RDG_EVENT_NAME("Integrate %ux%ux%u", TranslucencyGridSize.X, TranslucencyGridSize.Y, TranslucencyGridSize.Z),
					ComputePassFlags,
					ComputeShader,
					PassParameters,
					GroupSize);
			}

			if (View.ViewState && !View.bStatePrevViewInfoIsReadOnly)
			{
				View.ViewState->Lumen.TranslucencyVolume0 = GraphBuilder.ConvertToExternalTexture(TranslucencyGIVolumeNewHistory0);
				View.ViewState->Lumen.TranslucencyVolume1 = GraphBuilder.ConvertToExternalTexture(TranslucencyGIVolumeNewHistory1);
			}

			View.GetOwnLumenTranslucencyGIVolume().Texture0 = TranslucencyGIVolume0;
			View.GetOwnLumenTranslucencyGIVolume().Texture1 = TranslucencyGIVolume1;

			View.GetOwnLumenTranslucencyGIVolume().HistoryTexture0 = TranslucencyGIVolumeNewHistory0;
			View.GetOwnLumenTranslucencyGIVolume().HistoryTexture1 = TranslucencyGIVolumeNewHistory1;

			View.GetOwnLumenTranslucencyGIVolume().GridZParams = (FVector)VolumeParameters.TranslucencyGIGridZParams;
			View.GetOwnLumenTranslucencyGIVolume().GridPixelSizeShift = FMath::FloorLog2(CVarTranslucencyFroxelGridPixelSize.GetValueOnRenderThread());
			View.GetOwnLumenTranslucencyGIVolume().GridSize = TranslucencyGridSize;
		}
	}
}
