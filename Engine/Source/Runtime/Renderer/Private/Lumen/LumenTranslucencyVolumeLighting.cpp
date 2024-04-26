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

int32 GLumenTranslucencyVolume = 1;
FAutoConsoleVariableRef CVarLumenTranslucencyVolume(
	TEXT("r.Lumen.TranslucencyVolume.Enable"),
	GLumenTranslucencyVolume,
	TEXT(""),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

int32 GLumenTranslucencyVolumeTraceFromVolume = 1;
FAutoConsoleVariableRef CVarLumenTranslucencyVolumeTraceFromVolume(
	TEXT("r.Lumen.TranslucencyVolume.TraceFromVolume"),
	GLumenTranslucencyVolumeTraceFromVolume,
	TEXT("Whether to ray trace from the translucency volume's voxels to gather indirect lighting.  Only makes sense to disable if TranslucencyVolume.RadianceCache is enabled."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

int32 GTranslucencyFroxelGridPixelSize = 32;
FAutoConsoleVariableRef CVarTranslucencyFroxelGridPixelSize(
	TEXT("r.Lumen.TranslucencyVolume.GridPixelSize"),
	GTranslucencyFroxelGridPixelSize,
	TEXT("Size of a cell in the translucency grid, in pixels."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

float GTranslucencyGridDistributionLogZScale = .01f;
FAutoConsoleVariableRef CVarTranslucencyGridDistributionLogZScale(
	TEXT("r.Lumen.TranslucencyVolume.GridDistributionLogZScale"),
	GTranslucencyGridDistributionLogZScale,
	TEXT(""),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

float GTranslucencyGridDistributionLogZOffset = 1.0f;
FAutoConsoleVariableRef CVarTranslucencyGridDistributionLogZOffset(
	TEXT("r.Lumen.TranslucencyVolume.GridDistributionLogZOffset"),
	GTranslucencyGridDistributionLogZOffset,
	TEXT(""),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

float GTranslucencyGridDistributionZScale = 4.0f;
FAutoConsoleVariableRef CVarTranslucencyGridDistributionZScale(
	TEXT("r.Lumen.TranslucencyVolume.GridDistributionZScale"),
	GTranslucencyGridDistributionZScale,
	TEXT(""),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

float GTranslucencyGridEndDistanceFromCamera = 8000;
FAutoConsoleVariableRef CVarTranslucencyGridEndDistanceFromCamera(
	TEXT("r.Lumen.TranslucencyVolume.EndDistanceFromCamera"),
	GTranslucencyGridEndDistanceFromCamera,
	TEXT(""),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

int32 GTranslucencyVolumeSpatialFilter = 1;
FAutoConsoleVariableRef CVarTranslucencyVolumeSpatialFilter(
	TEXT("r.Lumen.TranslucencyVolume.SpatialFilter"),
	GTranslucencyVolumeSpatialFilter,
	TEXT("Whether to use a spatial filter on the volume traces."),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

int32 GTranslucencyVolumeSpatialFilterSampleCount = 3;
FAutoConsoleVariableRef CVarTranslucencyVolumeSpatialFilterSampleCount(
	TEXT("r.Lumen.TranslucencyVolume.SpatialFilter.SampleCount"),
	GTranslucencyVolumeSpatialFilterSampleCount,
	TEXT("When r.Lumen.TranslucencyVolume.SpatialFilter.Mode=1, this controls the effective sample count of the separable filter; that will be SampleCount*2+1. Default to a [-3,3] filter of 7 sample."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

float GTranslucencyVolumeSpatialFilterStandardDeviation = 5.0f; // default to not being a sharp filter
FAutoConsoleVariableRef CVarTranslucencyVolumeSpatialFilterStandardDeviation(
	TEXT("r.Lumen.TranslucencyVolume.SpatialFilter.StandardDeviation"),
	GTranslucencyVolumeSpatialFilterStandardDeviation,
	TEXT("When r.Lumen.TranslucencyVolume.SpatialFilter.Mode=1, The standard deviation of the Gaussian filter in Pixel. If a large value, the filter will become a cube filter. While when getting closer to 0, the filter will become a sharper Gaussian filter. Default to 5 meaning not a sharp flilter, close to a box filter for the default SampleCount of 3."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

int32 GTranslucencyVolumeTemporalReprojection = 1;
FAutoConsoleVariableRef CVarTranslucencyVolumeTemporalReprojection(
	TEXT("r.Lumen.TranslucencyVolume.TemporalReprojection"),
	GTranslucencyVolumeTemporalReprojection,
	TEXT("Whether to use temporal reprojection."),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

int32 GTranslucencyVolumeJitter = 1;
FAutoConsoleVariableRef CVarTranslucencyVolumeJitter(
	TEXT("r.Lumen.TranslucencyVolume.Temporal.Jitter"),
	GTranslucencyVolumeJitter,
	TEXT("Whether to apply jitter to each frame's translucency GI computation, achieving temporal super sampling."),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

float GTranslucencyVolumeHistoryWeight = .9f;
FAutoConsoleVariableRef CVarTranslucencyVolumeHistoryWeight(
	TEXT("r.Lumen.TranslucencyVolume.Temporal.HistoryWeight"),
	GTranslucencyVolumeHistoryWeight,
	TEXT("How much the history value should be weighted each frame.  This is a tradeoff between visible jittering and responsiveness."),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

TAutoConsoleVariable<int32> CVarLumenTranslucencyVolumeTemporalMaxRayDirections(
	TEXT("r.Lumen.TranslucencyVolume.Temporal.MaxRayDirections"),
	8,
	TEXT("Number of possible random directions from froxel center when sampling the lumen scene."),
	ECVF_Scalability | ECVF_RenderThreadSafe);

float GTranslucencyVolumeTraceStepFactor = 2;
FAutoConsoleVariableRef CVarTranslucencyVolumeTraceStepFactor(
	TEXT("r.Lumen.TranslucencyVolume.TraceStepFactor"),
	GTranslucencyVolumeTraceStepFactor,
	TEXT("."),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

int32 GTranslucencyVolumeTracingOctahedronResolution = 3;
FAutoConsoleVariableRef CVarTranslucencyVolumeTracingOctahedronResolution(
	TEXT("r.Lumen.TranslucencyVolume.TracingOctahedronResolution"),
	GTranslucencyVolumeTracingOctahedronResolution,
	TEXT("Resolution of the tracing octahedron.  Determines how many traces are done per voxel of the translucency lighting volume."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

float GTranslucencyVolumeVoxelTraceStartDistanceScale = 1.0f;
FAutoConsoleVariableRef CVarTranslucencyVoxelTraceStartDistanceScale(
	TEXT("r.Lumen.TranslucencyVolume.VoxelTraceStartDistanceScale"),
	GTranslucencyVolumeVoxelTraceStartDistanceScale,
	TEXT("."),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

float GTranslucencyVolumeMaxRayIntensity = 20.0f;
FAutoConsoleVariableRef CVarTranslucencyVolumeMaxRayIntensity(
	TEXT("r.Lumen.TranslucencyVolume.MaxRayIntensity"),
	GTranslucencyVolumeMaxRayIntensity,
	TEXT("."),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

int32 GLumenTranslucencyVolumeRadianceCache = 1;
FAutoConsoleVariableRef CVarLumenTranslucencyVolumeRadianceCache(
	TEXT("r.Lumen.TranslucencyVolume.RadianceCache"),
	GLumenTranslucencyVolumeRadianceCache,
	TEXT("Whether to use the Radiance Cache for Translucency"),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

int32 GTranslucencyVolumeRadianceCacheNumMipmaps = 3;
FAutoConsoleVariableRef CVarTranslucencyVolumeRadianceCacheNumMipmaps(
	TEXT("r.Lumen.TranslucencyVolume.RadianceCache.NumMipmaps"),
	GTranslucencyVolumeRadianceCacheNumMipmaps,
	TEXT("Number of radiance cache mipmaps."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

float GLumenTranslucencyVolumeRadianceCacheClipmapWorldExtent = 2500.0f;
FAutoConsoleVariableRef CVarLumenTranslucencyVolumeRadianceCacheClipmapWorldExtent(
	TEXT("r.Lumen.TranslucencyVolume.RadianceCache.ClipmapWorldExtent"),
	GLumenTranslucencyVolumeRadianceCacheClipmapWorldExtent,
	TEXT("World space extent of the first clipmap"),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

float GLumenTranslucencyVolumeRadianceCacheClipmapDistributionBase = 2.0f;
FAutoConsoleVariableRef CVarLumenTranslucencyVolumeRadianceCacheClipmapDistributionBase(
	TEXT("r.Lumen.TranslucencyVolume.RadianceCache.ClipmapDistributionBase"),
	GLumenTranslucencyVolumeRadianceCacheClipmapDistributionBase,
	TEXT("Base of the Pow() that controls the size of each successive clipmap relative to the first."),
	ECVF_RenderThreadSafe
);

int32 GTranslucencyVolumeRadianceCacheNumProbesToTraceBudget = 200;
FAutoConsoleVariableRef CVarTranslucencyVolumeRadianceCacheNumProbesToTraceBudget(
	TEXT("r.Lumen.TranslucencyVolume.RadianceCache.NumProbesToTraceBudget"),
	GTranslucencyVolumeRadianceCacheNumProbesToTraceBudget,
	TEXT(""),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

int32 GTranslucencyVolumeRadianceCacheGridResolution = 24;
FAutoConsoleVariableRef CVarTranslucencyVolumeRadianceCacheResolution(
	TEXT("r.Lumen.TranslucencyVolume.RadianceCache.GridResolution"),
	GTranslucencyVolumeRadianceCacheGridResolution,
	TEXT("Resolution of the probe placement grid within each clipmap"),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

int32 GTranslucencyVolumeRadianceCacheProbeResolution = 8;
FAutoConsoleVariableRef CVarTranslucencyVolumeRadianceCacheProbeResolution(
	TEXT("r.Lumen.TranslucencyVolume.RadianceCache.ProbeResolution"),
	GTranslucencyVolumeRadianceCacheProbeResolution,
	TEXT("Resolution of the probe's 2d radiance layout.  The number of rays traced for the probe will be ProbeResolution ^ 2"),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarTranslucencyVolumeRadianceCacheProbeAtlasResolutionInProbes(
	TEXT("r.Lumen.TranslucencyVolume.RadianceCache.ProbeAtlasResolutionInProbes"),
	128,
	TEXT("Number of probes along one dimension of the probe atlas cache texture. This controls the memory usage of the cache. Overflow currently results in incorrect rendering. Aligned to the next power of two."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

float GTranslucencyVolumeRadianceCacheReprojectionRadiusScale = 10.0f;
FAutoConsoleVariableRef CVarTranslucencyVolumeRadianceCacheProbeReprojectionRadiusScale(
	TEXT("r.Lumen.TranslucencyVolume.RadianceCache.ReprojectionRadiusScale"),
	GTranslucencyVolumeRadianceCacheReprojectionRadiusScale,
	TEXT(""),
	ECVF_RenderThreadSafe
);

int32 GTranslucencyVolumeRadianceCacheFarField = 0;
FAutoConsoleVariableRef CVarTranslucencyVolumeRadianceCacheFarField(
	TEXT("r.Lumen.TranslucencyVolume.RadianceCache.FarField"),
	GTranslucencyVolumeRadianceCacheFarField,
	TEXT("Whether to trace against the FarField representation"),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

int32 GTranslucencyVolumeRadianceCacheStats = 0;
FAutoConsoleVariableRef CVarTranslucencyVolumeRadianceCacheStats(
	TEXT("r.Lumen.TranslucencyVolume.RadianceCache.Stats"),
	GTranslucencyVolumeRadianceCacheStats,
	TEXT("GPU print out Radiance Cache update stats."),
	ECVF_RenderThreadSafe
);

int32 GTranslucencyVolumeRadianceCacheFrustumProbes = 0;
FAutoConsoleVariableRef CVarTranslucencyVolumeRadianceCacheFrustumProbes(
	TEXT("r.Lumen.TranslucencyVolume.RadianceCache.FrustumProbes"),
	GTranslucencyVolumeRadianceCacheFrustumProbes,
	TEXT("Enable the use of probes generated on view fruxtum froxels as radiance cache, instead of using a worls space radiance cache."),
	ECVF_RenderThreadSafe
);

int32 GTranslucencyVolumeRadianceCacheFrustumProbesLowResProbeResolution = 6;
FAutoConsoleVariableRef CVarTranslucencyVolumeRadianceCacheFrustumLowResProbesProbeResolution(
	TEXT("r.Lumen.TranslucencyVolume.RadianceCache.FrustumProbes.LowResProbeResolution"),
	GTranslucencyVolumeRadianceCacheFrustumProbesLowResProbeResolution,
	TEXT("Low resolution probes a re used to initialise the frustrum probes on camera cut or if temporal reprojection cannot happen. This is a warm up resolution before reprojection + temporal update happen. The number of rays traced for the probe will be ProbeResolution ^ 2. Must be within [4, 8]."),
	ECVF_RenderThreadSafe | ECVF_Scalability
);

int32 GTranslucencyVolumeRadianceCacheFrustumProbesProbeResolution = 16;
FAutoConsoleVariableRef CVarTranslucencyVolumeRadianceCacheFrustumProbesProbeResolution(
	TEXT("r.Lumen.TranslucencyVolume.RadianceCache.FrustumProbes.ProbeResolution"),
	GTranslucencyVolumeRadianceCacheFrustumProbesProbeResolution,
	TEXT("Resolution of the frustum probes's 2d radiance layout.  The number of rays traced for the probe will be ProbeResolution ^ 2. Must be within [4, 64]."),
	ECVF_RenderThreadSafe | ECVF_Scalability
);

int32 GTranslucencyVolumeRadianceCacheFrustumProbesFroxelSize = 4;
FAutoConsoleVariableRef CVarTranslucencyVolumeRadianceCacheFrustumProbesFroxelSize(
	TEXT("r.Lumen.TranslucencyVolume.RadianceCache.FrustumProbes.FroxelSize"),
	GTranslucencyVolumeRadianceCacheFrustumProbesFroxelSize,
	TEXT("Size of a frustum probes in the translucency froxel grid, in froxel."),
	ECVF_RenderThreadSafe | ECVF_Scalability
);

int32 GTranslucencyVolumeRadianceCacheFrustumProbesRefineTracePerFrame = 2;
FAutoConsoleVariableRef CVarTranslucencyVolumeRadianceCacheFrustumProbesRefineTracePerFrame(
	TEXT("r.Lumen.TranslucencyVolume.RadianceCache.FrustumProbes.RefineTracePerFrame"),
	GTranslucencyVolumeRadianceCacheFrustumProbesRefineTracePerFrame,
	TEXT("Size of a frustum probes in the translucency froxel grid, in froxel. Must be within [1, 8]."),
	ECVF_RenderThreadSafe | ECVF_Scalability
);

int32 GTranslucencyVolumeRadianceCacheFrustumProbesDebug = 0;
FAutoConsoleVariableRef CVarTranslucencyVolumeRadianceCacheFrustumProbesDebug(
	TEXT("r.Lumen.TranslucencyVolume.RadianceCache.FrustumProbes.Debug"),
	GTranslucencyVolumeRadianceCacheFrustumProbesDebug,
	TEXT("Print debug information about the trace frustum probe froxel."),
	ECVF_RenderThreadSafe
);

float GTranslucencyVolumeGridCenterOffsetFromDepthBuffer = 0.5f;
FAutoConsoleVariableRef CVarTranslucencyVolumeGridCenterOffsetFromDepthBuffer(
	TEXT("r.Lumen.TranslucencyVolume.GridCenterOffsetFromDepthBuffer"),
	GTranslucencyVolumeGridCenterOffsetFromDepthBuffer,
	TEXT("Offset in grid units to move grid center sample out form the depth buffer along the Z direction. -1 means disabled. This reduces sample self intersection with geometry when tracing the global distance field buffer, and thus reduces flickering in those areas, as well as results in less leaking sometimes. Set to -1 to disable."),
	ECVF_RenderThreadSafe
);

float GTranslucencyVolumeOffsetThresholdToAcceptDepthBufferOffset = 1.0f;
FAutoConsoleVariableRef CVarTranslucencyVolumeOffsetThresholdToAcceptDepthBufferOffset(
	TEXT("r.Lumen.TranslucencyVolume.OffsetThresholdToAcceptDepthBufferOffset"),
	GTranslucencyVolumeOffsetThresholdToAcceptDepthBufferOffset,
	TEXT("Offset in grid units to accept a sample to be moved forward in front of the depth buffer. This is to avoid moving all samples behind the depth buffer forward which would affect the lighting of translucent and volumetric at edges of mesh. Default to 1.0 to only allow moving the first layer of froxel intersecting depth."),
	ECVF_RenderThreadSafe
);

DECLARE_GPU_STAT(LumenTranslucencyVolumeLighting);

namespace LumenTranslucencyVolume
{
	float GetEndDistanceFromCamera(const FViewInfo& View)
	{
		// Ideally we'd use LumenSceneViewDistance directly, but direct shadowing via translucency lighting volume only covers 5000.0f units by default (r.TranslucencyLightingVolumeOuterDistance), 
		//		so there isn't much point covering beyond that.  
		const float ViewDistanceScale = FMath::Clamp(View.FinalPostProcessSettings.LumenSceneViewDistance / 20000.0f, .1f, 100.0f);
		return FMath::Clamp<float>(GTranslucencyGridEndDistanceFromCamera * ViewDistanceScale, 1.0f, 100000.0f);
	}
}

namespace LumenTranslucencyVolumeRadianceCache
{
	int32 GetNumClipmaps(float DistanceToCover)
	{
		int32 ClipmapIndex = 0;

		for (; ClipmapIndex < LumenRadianceCache::MaxClipmaps; ++ClipmapIndex)
		{
			const float ClipmapExtent = GLumenTranslucencyVolumeRadianceCacheClipmapWorldExtent * FMath::Pow(GLumenTranslucencyVolumeRadianceCacheClipmapDistributionBase, ClipmapIndex);

			if (ClipmapExtent > DistanceToCover)
			{
				break;
			}
		}

		return FMath::Clamp(ClipmapIndex + 1, 1, LumenRadianceCache::MaxClipmaps);
	}

	int32 GetClipmapGridResolution()
	{
		const int32 GridResolution = GTranslucencyVolumeRadianceCacheGridResolution;
		return FMath::Clamp(GridResolution, 1, 256);
	}

	int32 GetProbeResolution()
	{
		return GTranslucencyVolumeRadianceCacheProbeResolution;
	}

	int32 GetNumMipmaps()
	{
		return GTranslucencyVolumeRadianceCacheNumMipmaps;
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
		Parameters.ReprojectionRadiusScale = GTranslucencyVolumeRadianceCacheReprojectionRadiusScale;
		Parameters.ClipmapWorldExtent = GLumenTranslucencyVolumeRadianceCacheClipmapWorldExtent;
		Parameters.ClipmapDistributionBase = GLumenTranslucencyVolumeRadianceCacheClipmapDistributionBase;
		Parameters.RadianceProbeClipmapResolution = GetClipmapGridResolution();
		Parameters.ProbeAtlasResolutionInProbes = FIntPoint(GetProbeAtlasResolutionInProbes(), GetProbeAtlasResolutionInProbes());
		Parameters.NumRadianceProbeClipmaps = GetNumClipmaps(LumenTranslucencyVolume::GetEndDistanceFromCamera(View));
		Parameters.RadianceProbeResolution = FMath::Max(GetProbeResolution(), LumenRadianceCache::MinRadianceProbeResolution);
		Parameters.FinalProbeResolution = GetFinalProbeResolution();
		Parameters.FinalRadianceAtlasMaxMip = GetNumMipmaps() - 1;
		const float TraceBudgetScale = View.Family->bCurrentlyBeingEdited ? 10.0f : 1.0f;
		Parameters.NumProbesToTraceBudget = GTranslucencyVolumeRadianceCacheNumProbesToTraceBudget * TraceBudgetScale;
		Parameters.RadianceCacheStats = GTranslucencyVolumeRadianceCacheStats;
		return Parameters;
	}
};

static bool GetVolumeRadianceCacheFrustumProbes()
{
	return GTranslucencyVolumeRadianceCacheFrustumProbes > 0;
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
	OutGridSizeZ = FMath::TruncToInt(FMath::Log2((FarPlane - NearPlane) * GTranslucencyGridDistributionLogZScale) * GTranslucencyGridDistributionZScale) + 1;
	OutZParams = FVector(GTranslucencyGridDistributionLogZScale, GTranslucencyGridDistributionLogZOffset, GTranslucencyGridDistributionZScale);
}

FVector TranslucencyVolumeTemporalRandom(uint32 FrameNumber)
{
	// Center of the voxel
	FVector RandomOffsetValue(.5f, .5f, .5f);

	if (GTranslucencyVolumeJitter)
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
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float3>, VolumeFroxelProbeRadiance)
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
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>,  FroxelClearCountBufferUAV)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint4>, FroxelClearListBufferUAV)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>,  FroxelLowResInitCountBufferUAV)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint4>, FroxelLowResInitListBufferUAV)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>,  FroxelReprojUpdateCountBufferUAV)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint4>, FroxelReprojUpdateListBufferUAV)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>,  FroxelRefineTraceRayCountBufferUAV)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint4>, FroxelRefineTraceRayListBufferUAV)
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
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, FroxelClearCountBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, FroxelClearDispatchIndirectBufferUAV)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, FroxelLowResInitCountBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, FroxelLowResTraceDispatchIndirectBufferUAV)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, FroxelLowResTraceDispatchIndirectBufferRTUAV)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, FroxelLowResCopyDispatchIndirectBufferUAV)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, FroxelReprojUpdateCountBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, FroxelReprojUpdateDispatchIndirectBufferUAV)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, FroxelRefineTraceRayCountBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, FroxelRefineTraceRayDispatchIndirectBufferUAV)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, FroxelRefineTraceRayDispatchIndirectBufferRTUAV)
		SHADER_PARAMETER_STRUCT_INCLUDE(ShaderPrint::FShaderParameters, ShaderPrintUniformBuffer)
		SHADER_PARAMETER(FIntPoint, RayTracingThreadGroupSize)
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
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float3>, VolumeFroxelProbeRadianceHistory)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>,  VolumeFroxelProbeHitDistanceHistory)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float3>, VolumeFroxelLowResProbeRadiance)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>,  VolumeFroxelLowResProbeHitDistance)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D<float3>, RWVolumeFroxelProbeRadiance)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D<float>, RWVolumeFroxelProbeHitDistance)
		SHADER_PARAMETER_STRUCT_INCLUDE(FLumenTranslucencyLightingVolumeParameters, VolumeParameters)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, FroxelUpdateCountBuffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(RWBuffer<uint4>, FroxelUpdateListBuffer)
		RDG_BUFFER_ACCESS(FroxelUpdateDispatchIndirectBuffer, ERHIAccess::IndirectArgs)
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


class FFroxelProbesRefineTraceRayCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FFroxelProbesRefineTraceRayCS)
	SHADER_USE_PARAMETER_STRUCT(FFroxelProbesRefineTraceRayCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D<float3>, RWVolumeFroxelProbeRadiance)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D<float>, RWVolumeFroxelProbeHitDistance)
		SHADER_PARAMETER_STRUCT_INCLUDE(FLumenTranslucencyLightingVolumeParameters, VolumeParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(FLumenTranslucencyLightingVolumeTraceSetupParameters, TraceSetupParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(FLumenCardTracingParameters, TracingParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(LumenRadianceCache::FRadianceCacheInterpolationParameters, RadianceCacheParameters)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, FroxelRefineTraceRayCountBuffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(RWBuffer<uint4>, FroxelRefineTraceRayListBuffer)
		RDG_BUFFER_ACCESS(FroxelRefineTraceRayDispatchIndirectBuffer, ERHIAccess::IndirectArgs)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSceneTextureUniformParameters, SceneTexturesStruct)
	END_SHADER_PARAMETER_STRUCT()

	class FDynamicSkyLight : SHADER_PERMUTATION_BOOL("ENABLE_DYNAMIC_SKY_LIGHT");
	using FPermutationDomain = TShaderPermutationDomain<FDynamicSkyLight>;

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
		OutEnvironment.CompilerFlags.Add(CFLAG_Wave32);
	}
};

IMPLEMENT_GLOBAL_SHADER(FFroxelProbesRefineTraceRayCS, "/Engine/Private/Lumen/LumenTranslucencyVolumeLighting.usf", "FroxelProbesRefineTraceRayCS", SF_Compute);


#if RHI_RAYTRACING

class FFroxelProbesRefineHWRayTracing : public FLumenHardwareRayTracingShaderBase 
{
	DECLARE_LUMEN_RAYTRACING_SHADER(FFroxelProbesRefineHWRayTracing)

	class FDynamicSkyLight : SHADER_PERMUTATION_BOOL("ENABLE_DYNAMIC_SKY_LIGHT");
	using FPermutationDomain = TShaderPermutationDomain<FLumenHardwareRayTracingShaderBase::FBasePermutationDomain, FDynamicSkyLight>;

	// Parameters, note: we cannot use FTranslucencyVolumeTraceFroxelProbesCS::FParameters because SharedParameters already includes lots of data itself in different form.
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D<float3>, RWVolumeFroxelProbeRadiance)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D<float>, RWVolumeFroxelProbeHitDistance)
		SHADER_PARAMETER_STRUCT_INCLUDE(FLumenHardwareRayTracingShaderBase::FSharedParameters, SharedParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(LumenRadianceCache::FRadianceCacheInterpolationParameters, RadianceCacheParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(FLumenTranslucencyLightingVolumeParameters, VolumeParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(FLumenTranslucencyLightingVolumeTraceSetupParameters, TraceSetupParameters)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, FroxelRefineTraceRayCountBuffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(RWBuffer<uint4>, FroxelRefineTraceRayListBuffer)
		RDG_BUFFER_ACCESS(FroxelRefineTraceRayDispatchIndirectBuffer, ERHIAccess::IndirectArgs)
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

IMPLEMENT_LUMEN_RAYGEN_AND_COMPUTE_RAYTRACING_SHADERS(FFroxelProbesRefineHWRayTracing)

IMPLEMENT_GLOBAL_SHADER(FFroxelProbesRefineHWRayTracingRGS, "/Engine/Private/Lumen/LumenTranslucencyVolumeLighting.usf", "FroxelProbesRefineHWRayTracingRGS", SF_RayGen);
IMPLEMENT_GLOBAL_SHADER(FFroxelProbesRefineHWRayTracingCS,  "/Engine/Private/Lumen/LumenTranslucencyVolumeLighting.usf", "FroxelProbesRefineHWRayTracingCS",  SF_Compute);

#endif // RHI_RAYTRACING


class FTranslucencyVolumeTraceFroxelProbesCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FTranslucencyVolumeTraceFroxelProbesCS)
	SHADER_USE_PARAMETER_STRUCT(FTranslucencyVolumeTraceFroxelProbesCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FLumenCardTracingParameters, TracingParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(LumenRadianceCache::FRadianceCacheInterpolationParameters, RadianceCacheParameters)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D<float3>, RWVolumeTraceRadiance)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D<float>, RWVolumeTraceHitDistance)
		SHADER_PARAMETER_STRUCT_INCLUDE(FLumenTranslucencyLightingVolumeParameters, VolumeParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(FLumenTranslucencyLightingVolumeTraceSetupParameters, TraceSetupParameters)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSceneTextureUniformParameters, SceneTexturesStruct)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, FroxelLowResInitCountBuffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(RWBuffer<uint4>, FroxelLowResInitListBuffer)
		RDG_BUFFER_ACCESS(FroxelLowResTraceDispatchIndirectBuffer, ERHIAccess::IndirectArgs)
	END_SHADER_PARAMETER_STRUCT()

	class FDynamicSkyLight : SHADER_PERMUTATION_BOOL("ENABLE_DYNAMIC_SKY_LIGHT");
	class FSimpleCoverageBasedExpand : SHADER_PERMUTATION_BOOL("GLOBALSDF_SIMPLE_COVERAGE_BASED_EXPAND");
	class FLowResFroxelIndirect : SHADER_PERMUTATION_BOOL("PERMUTATION_LOWRES_FROXEL_INDIRECT");
	using FPermutationDomain = TShaderPermutationDomain<FDynamicSkyLight, FSimpleCoverageBasedExpand, FLowResFroxelIndirect>;

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
		OutEnvironment.CompilerFlags.Add(CFLAG_Wave32);
	}
};

IMPLEMENT_GLOBAL_SHADER(FTranslucencyVolumeTraceFroxelProbesCS, "/Engine/Private/Lumen/LumenTranslucencyVolumeLighting.usf", "TranslucencyVolumeFroxelProbesCS", SF_Compute);


#if RHI_RAYTRACING

class FTranslucencyVolumeFroxelLowResProbesRayTracing : public FLumenHardwareRayTracingShaderBase 
{
	DECLARE_LUMEN_RAYTRACING_SHADER(FTranslucencyVolumeFroxelLowResProbesRayTracing)

	class FDynamicSkyLight : SHADER_PERMUTATION_BOOL("ENABLE_DYNAMIC_SKY_LIGHT");
	using FPermutationDomain = TShaderPermutationDomain<FLumenHardwareRayTracingShaderBase::FBasePermutationDomain, FDynamicSkyLight>;

	// Parameters, note: we cannot use FTranslucencyVolumeTraceFroxelProbesCS::FParameters because SharedParameters already includes lots of data itself in different form.
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D<float3>, RWVolumeTraceRadiance)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D<float>, RWVolumeTraceHitDistance)
		SHADER_PARAMETER_STRUCT_INCLUDE(FLumenHardwareRayTracingShaderBase::FSharedParameters, SharedParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(LumenRadianceCache::FRadianceCacheInterpolationParameters, RadianceCacheParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(FLumenTranslucencyLightingVolumeParameters, VolumeParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(FLumenTranslucencyLightingVolumeTraceSetupParameters, TraceSetupParameters)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, FroxelLowResInitCountBuffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(RWBuffer<uint4>, FroxelLowResInitListBuffer)
		RDG_BUFFER_ACCESS(FroxelLowResTraceDispatchIndirectBuffer, ERHIAccess::IndirectArgs)
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

IMPLEMENT_LUMEN_RAYGEN_AND_COMPUTE_RAYTRACING_SHADERS(FTranslucencyVolumeFroxelLowResProbesRayTracing)

IMPLEMENT_GLOBAL_SHADER(FTranslucencyVolumeFroxelLowResProbesRayTracingRGS, "/Engine/Private/Lumen/LumenTranslucencyVolumeLighting.usf", "TranslucencyVolumeFroxelLowResProbesRayTracingRGS", SF_RayGen);
IMPLEMENT_GLOBAL_SHADER(FTranslucencyVolumeFroxelLowResProbesRayTracingCS,  "/Engine/Private/Lumen/LumenTranslucencyVolumeLighting.usf", "TranslucencyVolumeFroxelLowResProbesRayTracingCS",  SF_Compute);

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
	const int32 TranslucencyFroxelGridPixelSize = FMath::Max(1, GTranslucencyFroxelGridPixelSize);
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
	Parameters.GridCenterOffsetFromDepthBuffer = GTranslucencyVolumeGridCenterOffsetFromDepthBuffer;
	Parameters.GridCenterOffsetThresholdToAcceptDepthBufferOffset = FMath::Max(0, GTranslucencyVolumeOffsetThresholdToAcceptDepthBufferOffset);
	Parameters.FroxelDirectionJitterFrameIndex = GTranslucencyVolumeJitter ? int32(ViewStateFrameIndex % FMath::Max(1, CVarLumenTranslucencyVolumeTemporalMaxRayDirections.GetValueOnRenderThread())) : -1;

	Parameters.BlueNoise = CreateUniformBufferImmediate(GetBlueNoiseGlobalParameters(), EUniformBufferUsage::UniformBuffer_SingleDraw);
		
	Parameters.TranslucencyVolumeTracingOctahedronResolution = GTranslucencyVolumeTracingOctahedronResolution;

	// Froxel probes
	Parameters.TranslucencyVolumeTracingFroxelLowResProbesOctahedronResolution = FMath::Clamp(GTranslucencyVolumeRadianceCacheFrustumProbesLowResProbeResolution, 2, 8);
	Parameters.TranslucencyVolumeTracingFroxelProbesOctahedronResolution = FMath::Clamp(GTranslucencyVolumeRadianceCacheFrustumProbesProbeResolution, 4, 64);
	const uint32 FrustomProbeFroxelSize = FMath::Max(1u, (uint32)GTranslucencyVolumeRadianceCacheFrustumProbesFroxelSize);
	Parameters.TranslucencyVolumeTracingFroxelProbesFroxelSize = FUintVector(FrustomProbeFroxelSize, FrustomProbeFroxelSize, 1u); // No reduction of probe placement along depth
	Parameters.TranslucencyVolumeTracingFroxelProbesGridSize = FUintVector::DivideAndRoundUp(FUintVector(TranslucencyGridSize), Parameters.TranslucencyVolumeTracingFroxelProbesFroxelSize);
	Parameters.TranslucencyVolumeTracingFroxelProbePixelSizeShift = FMath::FloorLog2(TranslucencyFroxelGridPixelSize * FrustomProbeFroxelSize);
	Parameters.TranslucencyVolumeTracingFroxelProbeHZBMipLevel = FMath::Max<float>((int32)FMath::FloorLog2(TranslucencyFroxelGridPixelSize * FrustomProbeFroxelSize) - 1, 0.0f);
	Parameters.TranslucencyVolumeTracingFroxelProbeRefineTraceCountPerFrame = FMath::Clamp(GTranslucencyVolumeRadianceCacheFrustumProbesRefineTracePerFrame, 1, 8);
	
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
	FRDGTextureRef VolumeFroxelProbeRadiance,
	ERDGPassFlags ComputePassFlags)
{
	FTranslucencyVolumeTraceVoxelsCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FTranslucencyVolumeTraceVoxelsCS::FParameters>();
	PassParameters->RWVolumeTraceRadiance = GraphBuilder.CreateUAV(VolumeTraceRadiance);
	PassParameters->RWVolumeTraceHitDistance = GraphBuilder.CreateUAV(VolumeTraceHitDistance);

	PassParameters->TracingParameters = TracingParameters;
	PassParameters->RadianceCacheParameters = RadianceCacheParameters;
	PassParameters->VolumeParameters = VolumeParameters;
	PassParameters->TraceSetupParameters = TraceSetupParameters;
	PassParameters->VolumeFroxelProbeRadiance = VolumeFroxelProbeRadiance;

	PassParameters->SceneTexturesStruct = View.GetSceneTextures().UniformBuffer;

	const bool bTraceFromVolume = GLumenTranslucencyVolumeTraceFromVolume != 0;

	FTranslucencyVolumeTraceVoxelsCS::FPermutationDomain PermutationVector;
	PermutationVector.Set<FTranslucencyVolumeTraceVoxelsCS::FDynamicSkyLight>(bDynamicSkyLight);
	PermutationVector.Set<FTranslucencyVolumeTraceVoxelsCS::FProbeSourceMode>(VolumeFroxelProbeRadiance != nullptr ? 2 : (RadianceCacheParameters.RadianceProbeIndirectionTexture != nullptr ? 1 : 0));
	PermutationVector.Set<FTranslucencyVolumeTraceVoxelsCS::FTraceFromVolume>(bTraceFromVolume);
	PermutationVector.Set<FTranslucencyVolumeTraceVoxelsCS::FSimpleCoverageBasedExpand>(bTraceFromVolume && Lumen::UseGlobalSDFSimpleCoverageBasedExpand());
	auto ComputeShader = View.ShaderMap->GetShader<FTranslucencyVolumeTraceVoxelsCS>(PermutationVector);

	const FIntVector GroupSize = FComputeShaderUtils::GetGroupCount(VolumeTraceRadiance->Desc.GetSize(), FTranslucencyVolumeTraceVoxelsCS::GetGroupSize());

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("%s %ux%u", bTraceFromVolume ? TEXT("TraceVoxels") : TEXT("RadianceCacheInterpolate"), GTranslucencyVolumeTracingOctahedronResolution, GTranslucencyVolumeTracingOctahedronResolution),
		ComputePassFlags,
		ComputeShader,
		PassParameters,
		GroupSize);
}

void TraceFroxelProbesTranslucencyVolume(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View,
	bool bDynamicSkyLight,
	const FLumenCardTracingParameters& TracingParameters,
	LumenRadianceCache::FRadianceCacheInterpolationParameters RadianceCacheParameters,
	FLumenTranslucencyLightingVolumeParameters VolumeParameters,
	FLumenTranslucencyLightingVolumeTraceSetupParameters TraceSetupParameters,
	FRDGTextureRef VolumeTraceRadiance,
	FRDGTextureRef VolumeTraceHitDistance,
	ERDGPassFlags ComputePassFlags,
	FRDGBufferRef FroxelLowResTraceDispatchIndirectBuffer = nullptr,
	FRDGBufferSRVRef FroxelLowResInitCountBufferSRV = nullptr,
	FRDGBufferSRVRef FroxelLowResInitListBufferSRV = nullptr)
{
	FTranslucencyVolumeTraceFroxelProbesCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FTranslucencyVolumeTraceFroxelProbesCS::FParameters>();
	PassParameters->RWVolumeTraceRadiance = GraphBuilder.CreateUAV(VolumeTraceRadiance);
	PassParameters->RWVolumeTraceHitDistance = GraphBuilder.CreateUAV(VolumeTraceHitDistance);

	PassParameters->TracingParameters = TracingParameters;
	PassParameters->RadianceCacheParameters = RadianceCacheParameters;
	PassParameters->VolumeParameters = VolumeParameters;
	PassParameters->TraceSetupParameters = TraceSetupParameters;

	// Indirect fill up data
	PassParameters->FroxelLowResTraceDispatchIndirectBuffer = FroxelLowResTraceDispatchIndirectBuffer;
	PassParameters->FroxelLowResInitCountBuffer = FroxelLowResInitCountBufferSRV;
	PassParameters->FroxelLowResInitListBuffer = FroxelLowResInitListBufferSRV;
	const bool bIndirectLowResProbeTracing = FroxelLowResTraceDispatchIndirectBuffer != nullptr;

	PassParameters->SceneTexturesStruct = View.GetSceneTextures().UniformBuffer;

	FTranslucencyVolumeTraceFroxelProbesCS::FPermutationDomain PermutationVector;
	PermutationVector.Set<FTranslucencyVolumeTraceFroxelProbesCS::FDynamicSkyLight>(bDynamicSkyLight);
	PermutationVector.Set<FTranslucencyVolumeTraceFroxelProbesCS::FSimpleCoverageBasedExpand>(Lumen::UseGlobalSDFSimpleCoverageBasedExpand());
	PermutationVector.Set<FTranslucencyVolumeTraceFroxelProbesCS::FLowResFroxelIndirect>(bIndirectLowResProbeTracing);
	auto ComputeShader = View.ShaderMap->GetShader<FTranslucencyVolumeTraceFroxelProbesCS>(PermutationVector);

	if (bIndirectLowResProbeTracing)
	{
		ClearUnusedGraphResources(ComputeShader, PassParameters);
		GraphBuilder.AddPass(
			RDG_EVENT_NAME("TraceLowResFroxelProbesInd"),
			PassParameters,
			ERDGPassFlags::Compute,
			[PassParameters, ComputeShader](FRHICommandList& RHICmdList)
			{
				FComputeShaderUtils::DispatchIndirect(RHICmdList, ComputeShader, *PassParameters, PassParameters->FroxelLowResTraceDispatchIndirectBuffer->GetIndirectRHICallBuffer(), 0);
			});
	}
	else
	{
		const FIntVector GroupSize = FComputeShaderUtils::GetGroupCount(VolumeTraceRadiance->Desc.GetSize(), FTranslucencyVolumeTraceFroxelProbesCS::GetGroupSize());

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("%s %ux%ux%u", TEXT("TraceFroxelProbes"), VolumeParameters.TranslucencyVolumeTracingFroxelProbesGridSize.X, VolumeParameters.TranslucencyVolumeTracingFroxelProbesGridSize.Y, VolumeParameters.TranslucencyVolumeTracingFroxelProbesGridSize.Z),
			ComputePassFlags,
			ComputeShader,
			PassParameters,
			GroupSize);
	}
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
	Configuration.bFarField = GTranslucencyVolumeRadianceCacheFarField != 0;

	FMarkUsedRadianceCacheProbes MarkUsedRadianceCacheProbesCallbacks;

	if (GLumenTranslucencyVolume && GLumenTranslucencyVolumeRadianceCache 
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
				FTranslucencyVolumeFroxelLowResProbesRayTracingRGS::FPermutationDomain PermutationVector;
				PermutationVector.Set<FTranslucencyVolumeFroxelLowResProbesRayTracingRGS::FDynamicSkyLight>(DynamicSkyLight > 0);
				TShaderRef<FTranslucencyVolumeFroxelLowResProbesRayTracingRGS> RayGenerationShader = View.ShaderMap->GetShader<FTranslucencyVolumeFroxelLowResProbesRayTracingRGS>(PermutationVector);

				OutRayGenShaders.Add(RayGenerationShader.GetRayTracingShader());
			}
			{
				FFroxelProbesRefineHWRayTracingRGS::FPermutationDomain PermutationVector;
				PermutationVector.Set<FFroxelProbesRefineHWRayTracingRGS::FDynamicSkyLight>(DynamicSkyLight > 0);
				TShaderRef<FFroxelProbesRefineHWRayTracingRGS> RayGenerationShader = View.ShaderMap->GetShader<FFroxelProbesRefineHWRayTracingRGS>(PermutationVector);

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
	RDG_GPU_STAT_SCOPE(GraphBuilder, LumenTranslucencyVolumeLighting);
	if (GLumenTranslucencyVolume)
	{
		RDG_EVENT_SCOPE(GraphBuilder, "TranslucencyVolumeLighting");

		const FMatrix44f UnjitteredPrevWorldToClip = FMatrix44f(View.PrevViewInfo.ViewMatrices.GetViewMatrix() * View.PrevViewInfo.ViewMatrices.ComputeProjectionNoAAMatrix());		// LWC_TODO: Precision loss?

		if (GLumenTranslucencyVolumeRadianceCache && !RadianceCacheParameters.RadianceProbeIndirectionTexture)
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
				TraceSetupParameters.StepFactor = FMath::Clamp(GTranslucencyVolumeTraceStepFactor, .1f, 10.0f);
				TraceSetupParameters.MaxTraceDistance = Lumen::GetMaxTraceDistance(View);
				TraceSetupParameters.VoxelTraceStartDistanceScale = GTranslucencyVolumeVoxelTraceStartDistanceScale;
				TraceSetupParameters.MaxRayIntensity = GTranslucencyVolumeMaxRayIntensity;
			}

			const FIntVector OctahedralAtlasSize(
				TranslucencyGridSize.X * GTranslucencyVolumeTracingOctahedronResolution, 
				TranslucencyGridSize.Y * GTranslucencyVolumeTracingOctahedronResolution,
				TranslucencyGridSize.Z);

			FRDGTextureDesc VolumeTraceRadianceDesc(FRDGTextureDesc::Create3D(OctahedralAtlasSize, PF_FloatRGB, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV));
			FRDGTextureDesc VolumeTraceHitDistanceDesc(FRDGTextureDesc::Create3D(OctahedralAtlasSize, PF_R16F, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV));
	
			FRDGTextureRef VolumeTraceRadiance = GraphBuilder.CreateTexture(VolumeTraceRadianceDesc, TEXT("Lumen.TranslucencyVolume.VolumeTraceRadiance"));
			FRDGTextureRef VolumeTraceHitDistance = GraphBuilder.CreateTexture(VolumeTraceHitDistanceDesc, TEXT("Lumen.TranslucencyVolume.VolumeTraceHitDistance"));

			FRDGTextureRef VolumeFroxelProbeRadiance = nullptr;
			FRDGTextureRef VolumeFroxelProbeHitDistance = nullptr;
			FRDGTextureRef VolumeFroxelLowResProbeRadiance = nullptr;
			FRDGTextureRef VolumeFroxelLowResProbeHitDistance = nullptr;
			if (GetVolumeRadianceCacheFrustumProbes())
			{
				// Cannot use PF_FloatRGB otherwise that can lead to temporal loss of energy as well as hue shift after reprojection.
				EPixelFormat RadiancePixelFormat = PF_FloatRGBA;

				const FIntVector FroxelProbeAtlasSize(
					VolumeParameters.TranslucencyVolumeTracingFroxelProbesGridSize.X * VolumeParameters.TranslucencyVolumeTracingFroxelProbesOctahedronResolution,
					VolumeParameters.TranslucencyVolumeTracingFroxelProbesGridSize.Y * VolumeParameters.TranslucencyVolumeTracingFroxelProbesOctahedronResolution,
					VolumeParameters.TranslucencyVolumeTracingFroxelProbesGridSize.Z);
				FRDGTextureDesc VolumeFroxelProbeRadianceDesc(FRDGTextureDesc::Create3D(FroxelProbeAtlasSize, RadiancePixelFormat, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV));
				FRDGTextureDesc VolumeFroxelProbeHitDistanceDesc(FRDGTextureDesc::Create3D(FroxelProbeAtlasSize, PF_R16F, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV));
				VolumeFroxelProbeRadiance    = GraphBuilder.CreateTexture(VolumeFroxelProbeRadianceDesc, TEXT("Lumen.TranslucencyVolume.FroxelProbeRadiance"));
				VolumeFroxelProbeHitDistance = GraphBuilder.CreateTexture(VolumeFroxelProbeHitDistanceDesc, TEXT("Lumen.TranslucencyVolume.FroxelProbeHitDistance"));

				const FIntVector FroxelProbeLowResAtlasSize(
					VolumeParameters.TranslucencyVolumeTracingFroxelProbesGridSize.X * VolumeParameters.TranslucencyVolumeTracingFroxelLowResProbesOctahedronResolution,
					VolumeParameters.TranslucencyVolumeTracingFroxelProbesGridSize.Y * VolumeParameters.TranslucencyVolumeTracingFroxelLowResProbesOctahedronResolution,
					VolumeParameters.TranslucencyVolumeTracingFroxelProbesGridSize.Z);
				FRDGTextureDesc VolumeFroxelLowResProbeRadianceDesc(FRDGTextureDesc::Create3D(FroxelProbeLowResAtlasSize, RadiancePixelFormat, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV));
				FRDGTextureDesc VolumeFroxelLowResProbeHitDistanceDesc(FRDGTextureDesc::Create3D(FroxelProbeLowResAtlasSize, PF_R16F, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV));
				VolumeFroxelLowResProbeRadiance = GraphBuilder.CreateTexture(VolumeFroxelLowResProbeRadianceDesc, TEXT("Lumen.TranslucencyVolume.LowResFroxelProbeRadiance"));
				VolumeFroxelLowResProbeHitDistance = GraphBuilder.CreateTexture(VolumeFroxelLowResProbeHitDistanceDesc, TEXT("Lumen.TranslucencyVolume.LowResFroxelProbeHitDistance"));

				const bool bDynamicSkyLight = Lumen::ShouldHandleSkyLight(Scene, ViewFamily);

				bool bInlineRayTracing = Lumen::UseHardwareInlineRayTracing(*View.Family);

				{
					// Create buffer required for the froxel probe update scheduling
					const uint32		FroxelCount = VolumeParameters.TranslucencyVolumeTracingFroxelProbesGridSize.X * VolumeParameters.TranslucencyVolumeTracingFroxelProbesGridSize.Y * VolumeParameters.TranslucencyVolumeTracingFroxelProbesGridSize.Z;

					const uint32 R32_ByteSize		= sizeof(uint32) * 1;
					const uint32 R16G16B16A16_ByteSize = sizeof(uint16) * 4;


					FRDGBufferRef		FroxelClearCountBuffer							= GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(R32_ByteSize, 8), TEXT("Lumen.FroxelClearCountBuffer"));
					FRDGBufferUAVRef	FroxelClearCountBufferUAV						= GraphBuilder.CreateUAV(FroxelClearCountBuffer, PF_R32_UINT);
					FRDGBufferSRVRef	FroxelClearCountBufferSRV						= GraphBuilder.CreateSRV(FroxelClearCountBuffer, PF_R32_UINT);

					FRDGBufferRef		FroxelClearListBuffer							= GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(R16G16B16A16_ByteSize, FroxelCount), TEXT("Lumen.FroxelClearListBuffer"));
					FRDGBufferUAVRef	FroxelClearListBufferUAV						= GraphBuilder.CreateUAV(FroxelClearListBuffer, PF_R16G16B16A16_UINT);
					FRDGBufferSRVRef	FroxelClearListBufferSRV						= GraphBuilder.CreateSRV(FroxelClearListBuffer, PF_R16G16B16A16_UINT);

					FRDGBufferRef		FroxelClearDispatchIndirectBuffer				= GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateIndirectDesc<FRHIDispatchIndirectParameters>(1), TEXT("Lumen.FroxelClearDispatchIndirectBuffer"));
					FRDGBufferUAVRef	FroxelClearDispatchIndirectBufferUAV			= GraphBuilder.CreateUAV(FroxelClearDispatchIndirectBuffer, PF_R32_UINT);
					FRDGBufferSRVRef	FroxelClearDispatchIndirectBufferSRV			= GraphBuilder.CreateSRV(FroxelClearDispatchIndirectBuffer, PF_R32_UINT);
					
					FRDGBufferRef		FroxelLowResInitCountBuffer						= GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(R32_ByteSize, 8), TEXT("Lumen.FroxelLowResInitCountBuffer"));
					FRDGBufferUAVRef	FroxelLowResInitCountBufferUAV					= GraphBuilder.CreateUAV(FroxelLowResInitCountBuffer, PF_R32_UINT);
					FRDGBufferSRVRef	FroxelLowResInitCountBufferSRV					= GraphBuilder.CreateSRV(FroxelLowResInitCountBuffer, PF_R32_UINT);

					FRDGBufferRef		FroxelLowResInitListBuffer						= GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(R16G16B16A16_ByteSize, FroxelCount), TEXT("Lumen.FroxelLowResInitListBuffer"));
					FRDGBufferUAVRef	FroxelLowResInitListBufferUAV					= GraphBuilder.CreateUAV(FroxelLowResInitListBuffer, PF_R16G16B16A16_UINT);
					FRDGBufferSRVRef	FroxelLowResInitListBufferSRV					= GraphBuilder.CreateSRV(FroxelLowResInitListBuffer, PF_R16G16B16A16_UINT);

					FRDGBufferRef		FroxelLowResTraceDispatchIndirectBuffer			= GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateIndirectDesc<FRHIDispatchIndirectParameters>(1), TEXT("Lumen.FroxelLowResTraceDispatchIndirectBuffer"));
					FRDGBufferUAVRef	FroxelLowResTraceDispatchIndirectBufferUAV		= GraphBuilder.CreateUAV(FroxelLowResTraceDispatchIndirectBuffer, PF_R32_UINT);
					FRDGBufferSRVRef	FroxelLowResTraceDispatchIndirectBufferSRV		= GraphBuilder.CreateSRV(FroxelLowResTraceDispatchIndirectBuffer, PF_R32_UINT);

					FRDGBufferRef		FroxelLowResTraceDispatchIndirectBufferRT		= GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateIndirectDesc<FRHIDispatchIndirectParameters>(1), TEXT("Lumen.FroxelLowResTraceDispatchIndirectBufferRT"));
					FRDGBufferUAVRef	FroxelLowResTraceDispatchIndirectBufferRTUAV	= GraphBuilder.CreateUAV(FroxelLowResTraceDispatchIndirectBufferRT, PF_R32_UINT);
					FRDGBufferSRVRef	FroxelLowResTraceDispatchIndirectBufferRTSRV	= GraphBuilder.CreateSRV(FroxelLowResTraceDispatchIndirectBufferRT, PF_R32_UINT);

					FRDGBufferRef		FroxelLowResCopyDispatchIndirectBuffer			= GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateIndirectDesc<FRHIDispatchIndirectParameters>(1), TEXT("Lumen.FroxelLowResCopyDispatchIndirectBuffer"));
					FRDGBufferUAVRef	FroxelLowResCopyDispatchIndirectBufferUAV		= GraphBuilder.CreateUAV(FroxelLowResCopyDispatchIndirectBuffer, PF_R32_UINT);
					FRDGBufferSRVRef	FroxelLowResCopyDispatchIndirectBufferSRV		= GraphBuilder.CreateSRV(FroxelLowResCopyDispatchIndirectBuffer, PF_R32_UINT);
					
					FRDGBufferRef		FroxelReprojUpdateCountBuffer					= GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(R32_ByteSize, 8), TEXT("Lumen.FroxelReprojUpdateCountBuffer"));
					FRDGBufferUAVRef	FroxelReprojUpdateCountBufferUAV				= GraphBuilder.CreateUAV(FroxelReprojUpdateCountBuffer, PF_R32_UINT);
					FRDGBufferSRVRef	FroxelReprojUpdateCountBufferSRV				= GraphBuilder.CreateSRV(FroxelReprojUpdateCountBuffer, PF_R32_UINT);

					FRDGBufferRef		FroxelReprojUpdateListBuffer					= GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(R16G16B16A16_ByteSize, FroxelCount), TEXT("Lumen.FroxelReprojUpdateListBuffer"));
					FRDGBufferUAVRef	FroxelReprojUpdateListBufferUAV					= GraphBuilder.CreateUAV(FroxelReprojUpdateListBuffer, PF_R16G16B16A16_UINT);
					FRDGBufferSRVRef	FroxelReprojUpdateListBufferSRV					= GraphBuilder.CreateSRV(FroxelReprojUpdateListBuffer, PF_R16G16B16A16_UINT);

					FRDGBufferRef		FroxelReprojUpdateDispatchIndirectBuffer		= GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateIndirectDesc<FRHIDispatchIndirectParameters>(1), TEXT("Lumen.FroxelReprojUpdateDispatchIndirectBuffer"));
					FRDGBufferUAVRef	FroxelReprojUpdateDispatchIndirectBufferUAV		= GraphBuilder.CreateUAV(FroxelReprojUpdateDispatchIndirectBuffer, PF_R32_UINT);
					FRDGBufferSRVRef	FroxelReprojUpdateDispatchIndirectBufferSRV		= GraphBuilder.CreateSRV(FroxelReprojUpdateDispatchIndirectBuffer, PF_R32_UINT);
					
					FRDGBufferRef		FroxelRefineTraceRayCountBuffer					= GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(R32_ByteSize, 8), TEXT("Lumen.FroxelRefineTraceRayCountBuffer"));
					FRDGBufferUAVRef	FroxelRefineTraceRayCountBufferUAV				= GraphBuilder.CreateUAV(FroxelRefineTraceRayCountBuffer, PF_R32_UINT);
					FRDGBufferSRVRef	FroxelRefineTraceRayCountBufferSRV				= GraphBuilder.CreateSRV(FroxelRefineTraceRayCountBuffer, PF_R32_UINT);

					FRDGBufferRef		FroxelRefineTraceRayListBuffer					= GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(R16G16B16A16_ByteSize, FroxelCount * VolumeParameters.TranslucencyVolumeTracingFroxelProbeRefineTraceCountPerFrame), TEXT("Lumen.FroxelRefineTraceRayListBuffer"));
					FRDGBufferUAVRef	FroxelRefineTraceRayListBufferUAV				= GraphBuilder.CreateUAV(FroxelRefineTraceRayListBuffer, PF_R16G16B16A16_UINT);
					FRDGBufferSRVRef	FroxelRefineTraceRayListBufferSRV				= GraphBuilder.CreateSRV(FroxelRefineTraceRayListBuffer, PF_R16G16B16A16_UINT);

					FRDGBufferRef		FroxelRefineTraceRayDispatchIndirectBuffer		= GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateIndirectDesc<FRHIDispatchIndirectParameters>(1), TEXT("Lumen.FroxelRefineTraceRayDispatchIndirectBuffer"));
					FRDGBufferUAVRef	FroxelRefineTraceRayDispatchIndirectBufferUAV	= GraphBuilder.CreateUAV(FroxelRefineTraceRayDispatchIndirectBuffer, PF_R32_UINT);
					FRDGBufferSRVRef	FroxelRefineTraceRayDispatchIndirectBufferSRV	= GraphBuilder.CreateSRV(FroxelRefineTraceRayDispatchIndirectBuffer, PF_R32_UINT);

					FRDGBufferRef		FroxelRefineTraceRayDispatchIndirectBufferRT = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateIndirectDesc<FRHIDispatchIndirectParameters>(1), TEXT("Lumen.FroxelRefineTraceRayDispatchIndirectBufferRT"));
					FRDGBufferUAVRef	FroxelRefineTraceRayDispatchIndirectBufferRTUAV = GraphBuilder.CreateUAV(FroxelRefineTraceRayDispatchIndirectBufferRT, PF_R32_UINT);
					FRDGBufferSRVRef	FroxelRefineTraceRayDispatchIndirectBufferRTSRV = GraphBuilder.CreateSRV(FroxelRefineTraceRayDispatchIndirectBufferRT, PF_R32_UINT);

					// Clear buffers that need to be cleared
					AddClearUAVPass(GraphBuilder, FroxelClearCountBufferUAV, 0);
					AddClearUAVPass(GraphBuilder, FroxelLowResInitCountBufferUAV, 0);
					AddClearUAVPass(GraphBuilder, FroxelReprojUpdateCountBufferUAV, 0);
					AddClearUAVPass(GraphBuilder, FroxelRefineTraceRayCountBufferUAV, 0);


					float ViewFroxelProbesHistoryPreExposure = 1.0f;
					FRDGTextureRef ViewFroxelProbesRadianceHistory = GSystemTextures.GetVolumetricBlackDummy(GraphBuilder);
					FRDGTextureRef ViewFroxelProbesDistanceHistory = GSystemTextures.GetVolumetricBlackDummy(GraphBuilder);
					if (View.ViewState && View.ViewState->Lumen.ViewFroxelProbesRadiance.IsValid())
					{
						ViewFroxelProbesRadianceHistory = GraphBuilder.RegisterExternalTexture(View.ViewState->Lumen.ViewFroxelProbesRadiance);
						ViewFroxelProbesDistanceHistory = GraphBuilder.RegisterExternalTexture(View.ViewState->Lumen.ViewFroxelProbesDistance);
						ViewFroxelProbesHistoryPreExposure = View.ViewState->Lumen.ViewFroxelProbesHistoryPreExposure;
					}
					const FVector2f ViewFroxelProbesHistoryPreExposureAndInv = FVector2f(ViewFroxelProbesHistoryPreExposure, ViewFroxelProbesHistoryPreExposure > 0.0f ? 1.0f / ViewFroxelProbesHistoryPreExposure : 1.0f);

					const bool bCameraCut = 
						!( View.ViewState
						&& !View.bCameraCut
						&& !View.bPrevTransformsReset
						&& ViewFamily.bRealtimeUpdate
						&& ViewFroxelProbesRadianceHistory
						&& ViewFroxelProbesRadianceHistory->Desc == VolumeFroxelProbeRadianceDesc);

					// Schedule froxel update
					{
						FFroxelProbesUpdateSchedulerCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FFroxelProbesUpdateSchedulerCS::FParameters>();
						PassParameters->View = View.ViewUniformBuffer;
						PassParameters->VolumeParameters = VolumeParameters;
						PassParameters->FroxelClearCountBufferUAV = FroxelClearCountBufferUAV;
						PassParameters->FroxelClearListBufferUAV = FroxelClearListBufferUAV;
						PassParameters->FroxelLowResInitCountBufferUAV = FroxelLowResInitCountBufferUAV;
						PassParameters->FroxelLowResInitListBufferUAV = FroxelLowResInitListBufferUAV;
						PassParameters->FroxelReprojUpdateCountBufferUAV = FroxelReprojUpdateCountBufferUAV;
						PassParameters->FroxelReprojUpdateListBufferUAV = FroxelReprojUpdateListBufferUAV;
						PassParameters->FroxelRefineTraceRayCountBufferUAV = FroxelRefineTraceRayCountBufferUAV;
						PassParameters->FroxelRefineTraceRayListBufferUAV = FroxelRefineTraceRayListBufferUAV;
						PassParameters->UnjitteredPrevWorldToClip = UnjitteredPrevWorldToClip;
						PassParameters->CameraCut = bCameraCut ? 1 : 0;

						// Generate the samples we need this frame using LFSR to make sure we cover the full set of pixels in a minimum amount of frames (true for power of two)
						if (View.ViewState)
						{
							const uint32 FroxelProbeLFSRStartState = View.ViewState->Lumen.FroxelProbeLFSRStartState;
							uint32& FroxelProbeLFSRState = View.ViewState->Lumen.FroxelProbeLFSRState;

							// TODO LFRS code could be put in its own h/cpp for encapsulation and reusability
							auto CalcLFSRValue = [&](uint32 Taps0, uint32 Taps1, uint32 Taps2, uint32 Taps3, uint32 Mask)
								{
									// LFSR loops over 2^m-1 values excluding 0. So we need to generate the last 2^m value. 
									// We use LFSRCache==0 to detect that and return the last mask value.
									if (FroxelProbeLFSRState == 0)
									{
										FroxelProbeLFSRState = FroxelProbeLFSRStartState;
										return Mask;
									}

									// Update the cach value.
									uint32 Tap = ((Taps0 & FroxelProbeLFSRState) == 0 ? 0 : 1) ^ ((Taps1 & FroxelProbeLFSRState) == 0 ? 0 : 1);
									if (Taps2 > 0)
									{
										Tap ^= ((Taps2 & FroxelProbeLFSRState) == 0 ? 0 : 1);
									}
									if (Taps3 > 0)
									{
										Tap ^= ((Taps3 & FroxelProbeLFSRState) == 0 ? 0 : 1);
									}
									FroxelProbeLFSRState = ((FroxelProbeLFSRState << 1) | Tap) & Mask;

									uint32 ValueToReturn = FroxelProbeLFSRState - 1;
									if (FroxelProbeLFSRState == FroxelProbeLFSRStartState)
									{
										FroxelProbeLFSRState = 0;
									}

									return ValueToReturn % Mask;
								};

							// LFSR works with power of two number of texel. This works well since we want to sample the probe 2D representation.
							// https://en.wikipedia.org/wiki/Linear-feedback_shift_register
							const uint32 PowerOfTwoProbeOctahedronRes = FMath::RoundUpToPowerOfTwo(VolumeParameters.TranslucencyVolumeTracingFroxelProbesOctahedronResolution);
							const uint32 LFSRTexelCount = PowerOfTwoProbeOctahedronRes * PowerOfTwoProbeOctahedronRes;
							check(LFSRTexelCount <= 4096);	// Corresponds to the maximum resolution of 64x64

							for (uint32 i = 0; i < VolumeParameters.TranslucencyVolumeTracingFroxelProbeRefineTraceCountPerFrame; ++i)
							{
								uint32 NewValue = 0;
								uint32 SquareResolution = 0;

								if (LFSRTexelCount == 4)			// 2 bits, 2x2 probe resolution
								{
									SquareResolution = 2;
									NewValue = CalcLFSRValue(1 << 1, 1 << 0, 0, 0, 0x3);
								}
								else if (LFSRTexelCount == 16)
								{
									SquareResolution = 4;
									NewValue = CalcLFSRValue(1 << 3, 1 << 2, 0, 0, 0xF);
								}
								else if (LFSRTexelCount == 64)
								{
									SquareResolution = 8;
									NewValue = CalcLFSRValue(1 << 5, 1 << 4, 0, 0, 0x3F);
								}
								else if (LFSRTexelCount == 256)
								{
									SquareResolution = 16;
									NewValue = CalcLFSRValue(1 << 7, 1 << 5, 1 << 4, 1 << 3, 0xFF);
								}
								else if (LFSRTexelCount == 1024)
								{
									SquareResolution = 32;
									NewValue = CalcLFSRValue(1 << 9, 1 << 6, 0, 0, 0x3FF);
								}
								else if (LFSRTexelCount == 4096)
								{
									SquareResolution = 64;
									NewValue = CalcLFSRValue(1 << 11, 1 << 10, 1 << 9, 1 << 3, 0xFFF);
								}
								else
								{
									check(false);	// this should not happen given a square with size of power of two.
								}

								uint32 CoordX = NewValue / SquareResolution;
								uint32 CoordY = NewValue - CoordX * SquareResolution;

								// Since the RoundUpToPowerOfTwo the probe resolution, the square of pixel we parse can be larger than the actual probe resolution, that is why we modulate by the ProbesOctahedronResolution.
								// This means that some texel will update at a high rate for some probe resolution.
								PassParameters->ProbeSamplesToTrace[i] = FUintVector4(CoordX % VolumeParameters.TranslucencyVolumeTracingFroxelProbesOctahedronResolution, CoordY % VolumeParameters.TranslucencyVolumeTracingFroxelProbesOctahedronResolution, 0, 0);
							}
						}
						else
						{
							// Default pixel update when no state is available, which should not happen anyway.
							static uint32 FallBackTracing = 0;
							for (uint32 i = 0; i < VolumeParameters.TranslucencyVolumeTracingFroxelProbeRefineTraceCountPerFrame; ++i)
							{
								uint32 CoordX = FallBackTracing / VolumeParameters.TranslucencyVolumeTracingFroxelProbesOctahedronResolution;
								uint32 CoordY = FallBackTracing - CoordX * VolumeParameters.TranslucencyVolumeTracingFroxelProbesOctahedronResolution;
								PassParameters->ProbeSamplesToTrace[i] = FUintVector4(CoordX % VolumeParameters.TranslucencyVolumeTracingFroxelProbesOctahedronResolution, CoordY % VolumeParameters.TranslucencyVolumeTracingFroxelProbesOctahedronResolution, 0, 0);
								FallBackTracing++;
							}
						}

						FFroxelProbesUpdateSchedulerCS::FPermutationDomain PermutationVector;
						auto ComputeShader = View.ShaderMap->GetShader<FFroxelProbesUpdateSchedulerCS>(PermutationVector);

						const FIntVector GroupSize = FComputeShaderUtils::GetGroupCount(FIntVector(VolumeParameters.TranslucencyVolumeTracingFroxelProbesGridSize), FTranslucencyVolumeTraceFroxelProbesCS::GetGroupSize());

						FComputeShaderUtils::AddPass(
							GraphBuilder,
							RDG_EVENT_NAME("%s %ux%ux%u", TEXT("FroxelProbesUpdateScheduler"), VolumeParameters.TranslucencyVolumeTracingFroxelProbesGridSize.X, VolumeParameters.TranslucencyVolumeTracingFroxelProbesGridSize.Y, VolumeParameters.TranslucencyVolumeTracingFroxelProbesGridSize.Z),
							ComputePassFlags | ERDGPassFlags::NeverCull,
							ComputeShader,
							PassParameters,
							GroupSize);
					}


					// Create indirect dispatch buffers
					{
						FFroxelProbesUpdateIndirectArgsSetupCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FFroxelProbesUpdateIndirectArgsSetupCS::FParameters>();
						PassParameters->VolumeParameters = VolumeParameters;
						PassParameters->FroxelClearCountBuffer = FroxelClearCountBufferSRV;
						PassParameters->FroxelClearDispatchIndirectBufferUAV = FroxelClearDispatchIndirectBufferUAV;
						PassParameters->FroxelLowResInitCountBuffer = FroxelLowResInitCountBufferSRV;
						PassParameters->FroxelLowResTraceDispatchIndirectBufferUAV = FroxelLowResTraceDispatchIndirectBufferUAV;
						PassParameters->FroxelLowResTraceDispatchIndirectBufferRTUAV = FroxelLowResTraceDispatchIndirectBufferRTUAV;
						PassParameters->FroxelLowResCopyDispatchIndirectBufferUAV = FroxelLowResCopyDispatchIndirectBufferUAV;
						PassParameters->FroxelReprojUpdateCountBuffer = FroxelReprojUpdateCountBufferSRV;
						PassParameters->FroxelReprojUpdateDispatchIndirectBufferUAV = FroxelReprojUpdateDispatchIndirectBufferUAV;
						PassParameters->FroxelRefineTraceRayCountBuffer = FroxelRefineTraceRayCountBufferSRV;
						PassParameters->FroxelRefineTraceRayDispatchIndirectBufferUAV = FroxelRefineTraceRayDispatchIndirectBufferUAV;
						PassParameters->FroxelRefineTraceRayDispatchIndirectBufferRTUAV = FroxelRefineTraceRayDispatchIndirectBufferRTUAV;
					#if RHI_RAYTRACING
						PassParameters->RayTracingThreadGroupSize = bInlineRayTracing ? FTranslucencyVolumeFroxelLowResProbesRayTracingCS::GetThreadGroupSize(View.GetShaderPlatform()) : FTranslucencyVolumeFroxelLowResProbesRayTracingRGS::GetThreadGroupSize();;
					#else
						PassParameters->RayTracingThreadGroupSize = FIntPoint(1, 1);
					#endif

						const bool bDebugFroxelProbesUpdateIndirectArgs = GTranslucencyVolumeRadianceCacheFrustumProbesDebug > 0;
						if (bDebugFroxelProbesUpdateIndirectArgs)
						{
							ShaderPrint::SetEnabled(true);
							ShaderPrint::RequestSpaceForCharacters(128);
							ShaderPrint::SetParameters(GraphBuilder, View.ShaderPrintData, PassParameters->ShaderPrintUniformBuffer);
						}

						FFroxelProbesUpdateIndirectArgsSetupCS::FPermutationDomain PermutationVector;
						PermutationVector.Set<FFroxelProbesUpdateIndirectArgsSetupCS::FDebugPrint>(bDebugFroxelProbesUpdateIndirectArgs);
						auto ComputeShader = View.ShaderMap->GetShader<FFroxelProbesUpdateIndirectArgsSetupCS>(PermutationVector);

						const FIntVector GroupSize = FIntVector(1, 1, 1);

						FComputeShaderUtils::AddPass(
							GraphBuilder,
							RDG_EVENT_NAME("FroxelProbesUpdateIndirectArgsSetup"),
							ComputePassFlags | ERDGPassFlags::NeverCull,
							ComputeShader,
							PassParameters,
							GroupSize);
					}


					// Trace low res froxel probes that needs to be initialised
				#if RHI_RAYTRACING
					if (Lumen::UseHardwareRayTracedTranslucencyVolume(ViewFamily))
					{
						FTranslucencyVolumeFroxelLowResProbesRayTracingRGS::FParameters* PassParameters = GraphBuilder.AllocParameters<FTranslucencyVolumeFroxelLowResProbesRayTracingRGS::FParameters>();

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
						PassParameters->FroxelLowResTraceDispatchIndirectBuffer = FroxelLowResTraceDispatchIndirectBufferRT;
						PassParameters->FroxelLowResInitCountBuffer = FroxelLowResInitCountBufferSRV;
						PassParameters->FroxelLowResInitListBuffer = FroxelLowResInitListBufferSRV;

						FTranslucencyVolumeFroxelLowResProbesRayTracingRGS::FPermutationDomain PermutationVector;
						PermutationVector.Set<FTranslucencyVolumeFroxelLowResProbesRayTracingRGS::FDynamicSkyLight>(bDynamicSkyLight);

						if (bInlineRayTracing)
						{
							FTranslucencyVolumeFroxelLowResProbesRayTracingCS::AddLumenRayTracingDispatchIndirect(
								GraphBuilder,
								RDG_EVENT_NAME("RayTraceLowResFroxelProbes HWRT Indirect (inline)"),
								View,
								PermutationVector,
								PassParameters,
								PassParameters->FroxelLowResTraceDispatchIndirectBuffer,
								0, // IndirectArgsOffset
								ComputePassFlags);
						}
						else
						{
							const bool bUseMinimalPayload = true;
							FTranslucencyVolumeFroxelLowResProbesRayTracingRGS::AddLumenRayTracingDispatchIndirect(
								GraphBuilder,
								RDG_EVENT_NAME("RayTraceLowResFroxelProbes HWRT Indirect"),
								View,
								PermutationVector,
								PassParameters,
								PassParameters->FroxelLowResTraceDispatchIndirectBuffer,
								0, // IndirectArgsOffset
								bUseMinimalPayload);
						}
					}
					else
				#endif // RHI_RAYTRACING
					{
						TraceFroxelProbesTranslucencyVolume(
							GraphBuilder,
							View,
							bDynamicSkyLight,
							TracingParameters,
							RadianceCacheParameters,
							VolumeParameters,
							TraceSetupParameters,
							VolumeFroxelLowResProbeRadiance,
							VolumeFroxelLowResProbeHitDistance,
							ComputePassFlags | ERDGPassFlags::NeverCull,
							FroxelLowResTraceDispatchIndirectBuffer,
							FroxelLowResInitCountBufferSRV,
							FroxelLowResInitListBufferSRV);
					}


					// Clear non visible froxels
					{
						FUpdateFroxelProbesCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FUpdateFroxelProbesCS::FParameters>();
						PassParameters->View = View.ViewUniformBuffer;
						PassParameters->VolumeFroxelProbeRadianceHistory = nullptr;
						PassParameters->VolumeFroxelProbeHitDistanceHistory = nullptr;
						PassParameters->VolumeFroxelLowResProbeRadiance = nullptr;
						PassParameters->VolumeFroxelLowResProbeHitDistance = nullptr;
						PassParameters->RWVolumeFroxelProbeRadiance = GraphBuilder.CreateUAV(VolumeFroxelProbeRadiance);
						PassParameters->RWVolumeFroxelProbeHitDistance = GraphBuilder.CreateUAV(VolumeFroxelProbeHitDistance);
					
						PassParameters->VolumeParameters = VolumeParameters;
						PassParameters->UnjitteredPrevWorldToClip = UnjitteredPrevWorldToClip;
						PassParameters->ViewFroxelProbesHistoryPreExposureAndInv = ViewFroxelProbesHistoryPreExposureAndInv;
					
						// Indirect fill up data
						PassParameters->FroxelUpdateDispatchIndirectBuffer = FroxelClearDispatchIndirectBuffer;
						PassParameters->FroxelUpdateCountBuffer = FroxelClearCountBufferSRV;
						PassParameters->FroxelUpdateListBuffer = FroxelClearListBufferSRV;
					
						FUpdateFroxelProbesCS::FPermutationDomain PermutationVector;
						PermutationVector.Set<FUpdateFroxelProbesCS::FProbeFillMode>(2); // Clear mode
						auto ComputeShader = View.ShaderMap->GetShader<FUpdateFroxelProbesCS>(PermutationVector);
					
						ClearUnusedGraphResources(ComputeShader, PassParameters);
						GraphBuilder.AddPass(
							RDG_EVENT_NAME("FroxelProbesClear"),
							PassParameters,
							ERDGPassFlags::Compute | ERDGPassFlags::NeverCull,
							[PassParameters, ComputeShader](FRHICommandList& RHICmdList)
							{
								FComputeShaderUtils::DispatchIndirect(RHICmdList, ComputeShader, *PassParameters, PassParameters->FroxelUpdateDispatchIndirectBuffer->GetIndirectRHICallBuffer(), 0);
							});
					}


					// Reproject previous data
					{
						FUpdateFroxelProbesCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FUpdateFroxelProbesCS::FParameters>();
						PassParameters->View = View.ViewUniformBuffer;
						PassParameters->VolumeFroxelProbeRadianceHistory = ViewFroxelProbesRadianceHistory;
						PassParameters->VolumeFroxelProbeHitDistanceHistory = ViewFroxelProbesDistanceHistory;
						PassParameters->VolumeFroxelLowResProbeRadiance = nullptr;
						PassParameters->VolumeFroxelLowResProbeHitDistance = nullptr;
						PassParameters->RWVolumeFroxelProbeRadiance = GraphBuilder.CreateUAV(VolumeFroxelProbeRadiance);
						PassParameters->RWVolumeFroxelProbeHitDistance = GraphBuilder.CreateUAV(VolumeFroxelProbeHitDistance);

						PassParameters->VolumeParameters = VolumeParameters;
						PassParameters->UnjitteredPrevWorldToClip = UnjitteredPrevWorldToClip;
						PassParameters->ViewFroxelProbesHistoryPreExposureAndInv = ViewFroxelProbesHistoryPreExposureAndInv;

						// Indirect fill up data
						PassParameters->FroxelUpdateDispatchIndirectBuffer = FroxelReprojUpdateDispatchIndirectBuffer;
						PassParameters->FroxelUpdateCountBuffer = FroxelReprojUpdateCountBufferSRV;
						PassParameters->FroxelUpdateListBuffer = FroxelReprojUpdateListBufferSRV;

						FUpdateFroxelProbesCS::FPermutationDomain PermutationVector;
						PermutationVector.Set<FUpdateFroxelProbesCS::FProbeFillMode>(0); // Reproject last frame
						auto ComputeShader = View.ShaderMap->GetShader<FUpdateFroxelProbesCS>(PermutationVector);

						ClearUnusedGraphResources(ComputeShader, PassParameters);
						GraphBuilder.AddPass(
							RDG_EVENT_NAME("FroxelProbesReproject"),
							PassParameters,
							ERDGPassFlags::Compute | ERDGPassFlags::NeverCull,
							[PassParameters, ComputeShader](FRHICommandList& RHICmdList)
							{
								FComputeShaderUtils::DispatchIndirect(RHICmdList, ComputeShader, *PassParameters, PassParameters->FroxelUpdateDispatchIndirectBuffer->GetIndirectRHICallBuffer(), 0);
							});
					}


					// Compose low res froxel probes on high res probes as init/reset
					{
						FUpdateFroxelProbesCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FUpdateFroxelProbesCS::FParameters>();
						PassParameters->View = View.ViewUniformBuffer;
						PassParameters->VolumeFroxelProbeRadianceHistory = nullptr;
						PassParameters->VolumeFroxelProbeHitDistanceHistory = nullptr;
						PassParameters->VolumeFroxelLowResProbeRadiance = VolumeFroxelLowResProbeRadiance;
						PassParameters->VolumeFroxelLowResProbeHitDistance = VolumeFroxelLowResProbeHitDistance;
						PassParameters->RWVolumeFroxelProbeRadiance = GraphBuilder.CreateUAV(VolumeFroxelProbeRadiance);
						PassParameters->RWVolumeFroxelProbeHitDistance = GraphBuilder.CreateUAV(VolumeFroxelProbeHitDistance);

						PassParameters->VolumeParameters = VolumeParameters;
						PassParameters->UnjitteredPrevWorldToClip = UnjitteredPrevWorldToClip;
						PassParameters->ViewFroxelProbesHistoryPreExposureAndInv = ViewFroxelProbesHistoryPreExposureAndInv;

						// Indirect fill up data
						PassParameters->FroxelUpdateDispatchIndirectBuffer = FroxelLowResCopyDispatchIndirectBuffer;
						PassParameters->FroxelUpdateCountBuffer = FroxelLowResInitCountBufferSRV;
						PassParameters->FroxelUpdateListBuffer = FroxelLowResInitListBufferSRV;

						FUpdateFroxelProbesCS::FPermutationDomain PermutationVector;
						PermutationVector.Set<FUpdateFroxelProbesCS::FProbeFillMode>(1); // Reset with low res probes
						auto ComputeShader = View.ShaderMap->GetShader<FUpdateFroxelProbesCS>(PermutationVector);

						ClearUnusedGraphResources(ComputeShader, PassParameters);
						GraphBuilder.AddPass(
							RDG_EVENT_NAME("FroxelProbesCopyLowResInit"),
							PassParameters,
							ERDGPassFlags::Compute | ERDGPassFlags::NeverCull,
							[PassParameters, ComputeShader](FRHICommandList& RHICmdList)
							{
								FComputeShaderUtils::DispatchIndirect(RHICmdList, ComputeShader, *PassParameters, PassParameters->FroxelUpdateDispatchIndirectBuffer->GetIndirectRHICallBuffer(), 0);
							});
					}


				#if RHI_RAYTRACING
					// Refine froxel probes using N rays per frame.
					if (Lumen::UseHardwareRayTracedTranslucencyVolume(ViewFamily))
					{
						FFroxelProbesRefineHWRayTracingRGS::FParameters* PassParameters = GraphBuilder.AllocParameters<FFroxelProbesRefineHWRayTracingRGS::FParameters>();

						PassParameters->RWVolumeFroxelProbeRadiance = GraphBuilder.CreateUAV(VolumeFroxelProbeRadiance);
						PassParameters->RWVolumeFroxelProbeHitDistance = GraphBuilder.CreateUAV(VolumeFroxelProbeHitDistance);

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
						PassParameters->FroxelRefineTraceRayDispatchIndirectBuffer = FroxelRefineTraceRayDispatchIndirectBufferRT;
						PassParameters->FroxelRefineTraceRayCountBuffer = FroxelRefineTraceRayCountBufferSRV;
						PassParameters->FroxelRefineTraceRayListBuffer = FroxelRefineTraceRayListBufferSRV;

						FFroxelProbesRefineHWRayTracingRGS::FPermutationDomain PermutationVector;
						PermutationVector.Set<FFroxelProbesRefineHWRayTracingRGS::FDynamicSkyLight>(bDynamicSkyLight);

						if (bInlineRayTracing)
						{
							FFroxelProbesRefineHWRayTracingCS::AddLumenRayTracingDispatchIndirect(
								GraphBuilder,
								RDG_EVENT_NAME("FroxelProbesRefineTraceRay HWRT (inline)"),
								View,
								PermutationVector,
								PassParameters,
								PassParameters->FroxelRefineTraceRayDispatchIndirectBuffer,
								0, // IndirectArgsOffset
								ComputePassFlags);
						}
						else
						{
							const bool bUseMinimalPayload = true;
							FFroxelProbesRefineHWRayTracingRGS::AddLumenRayTracingDispatchIndirect(
								GraphBuilder,
								RDG_EVENT_NAME("FroxelProbesRefineTraceRay HWRT"),
								View,
								PermutationVector,
								PassParameters,
								PassParameters->FroxelRefineTraceRayDispatchIndirectBuffer,
								0, // IndirectArgsOffset
								bUseMinimalPayload);
						}
					}
					else
				#endif // RHI_RAYTRACING
					{
						FFroxelProbesRefineTraceRayCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FFroxelProbesRefineTraceRayCS::FParameters>();
						PassParameters->RWVolumeFroxelProbeRadiance = GraphBuilder.CreateUAV(VolumeFroxelProbeRadiance);
						PassParameters->RWVolumeFroxelProbeHitDistance = GraphBuilder.CreateUAV(VolumeFroxelProbeHitDistance);

						PassParameters->VolumeParameters = VolumeParameters;
						PassParameters->TraceSetupParameters = TraceSetupParameters;
						PassParameters->TracingParameters = TracingParameters;
						PassParameters->RadianceCacheParameters = RadianceCacheParameters;
						PassParameters->SceneTexturesStruct = View.GetSceneTextures().UniformBuffer;

						// Indirect fill up data
						PassParameters->FroxelRefineTraceRayDispatchIndirectBuffer = FroxelRefineTraceRayDispatchIndirectBuffer;
						PassParameters->FroxelRefineTraceRayCountBuffer = FroxelRefineTraceRayCountBufferSRV;
						PassParameters->FroxelRefineTraceRayListBuffer = FroxelRefineTraceRayListBufferSRV;

						FFroxelProbesRefineTraceRayCS::FPermutationDomain PermutationVector;
						PermutationVector.Set<FFroxelProbesRefineTraceRayCS::FDynamicSkyLight>(bDynamicSkyLight);
						auto ComputeShader = View.ShaderMap->GetShader<FFroxelProbesRefineTraceRayCS>(PermutationVector);

						ClearUnusedGraphResources(ComputeShader, PassParameters);
						GraphBuilder.AddPass(
							RDG_EVENT_NAME("FroxelProbesRefineTraceRay"),
							PassParameters,
							ERDGPassFlags::Compute | ERDGPassFlags::NeverCull,
							[PassParameters, ComputeShader](FRHICommandList& RHICmdList)
							{
								FComputeShaderUtils::DispatchIndirect(RHICmdList, ComputeShader, *PassParameters, PassParameters->FroxelRefineTraceRayDispatchIndirectBuffer->GetIndirectRHICallBuffer(), 0);
							});
					}


					if (View.ViewState && !View.bStatePrevViewInfoIsReadOnly)
					{
						View.ViewState->Lumen.ViewFroxelProbesRadiance = GraphBuilder.ConvertToExternalTexture(VolumeFroxelProbeRadiance);
						View.ViewState->Lumen.ViewFroxelProbesDistance = GraphBuilder.ConvertToExternalTexture(VolumeFroxelProbeHitDistance);

						View.ViewState->Lumen.ViewFroxelProbesHistoryPreExposure = ViewFroxelProbesHistoryPreExposure;
					}
				}
			}

			if (Lumen::UseHardwareRayTracedTranslucencyVolume(ViewFamily) && GLumenTranslucencyVolumeTraceFromVolume != 0)
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
					VolumeFroxelProbeRadiance,
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
					VolumeFroxelProbeRadiance,
					ComputePassFlags);
			}

			if (GTranslucencyVolumeSpatialFilter)
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
					PassParameters->SpatialFilterSampleCount = FMath::Max(1, GTranslucencyVolumeSpatialFilterSampleCount);

					const float GaussianFilterStandardDev = FMath::Max(0.1, GTranslucencyVolumeSpatialFilterStandardDeviation);
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
					GTranslucencyVolumeTemporalReprojection
					&& View.ViewState
					&& !View.bCameraCut
					&& !View.bPrevTransformsReset
					&& ViewFamily.bRealtimeUpdate
					&& TranslucencyGIVolumeHistory0
					&& TranslucencyGIVolumeHistory0->Desc == LumenTranslucencyGIDesc0;

				PassParameters->HistoryWeight = GTranslucencyVolumeHistoryWeight;
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
			View.GetOwnLumenTranslucencyGIVolume().GridPixelSizeShift = FMath::FloorLog2(GTranslucencyFroxelGridPixelSize);
			View.GetOwnLumenTranslucencyGIVolume().GridSize = TranslucencyGridSize;
		}
	}
}
