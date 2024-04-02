// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	LumenTranslucencyVolumeHardwareRayTracing.cpp
=============================================================================*/

#include "LumenTranslucencyVolumeLighting.h"
#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "SceneUtils.h"
#include "PipelineStateCache.h"
#include "ShaderParameterStruct.h"
#include "VolumeLighting.h"
#include "LumenTracingUtils.h"
#include "LumenRadianceCache.h"

#if RHI_RAYTRACING

#include "RayTracing/RaytracingOptions.h"
#include "RayTracing/RayTracingLighting.h"
#include "LumenHardwareRayTracingCommon.h"

// Console variables
static TAutoConsoleVariable<int32> CVarLumenTranslucencyVolumeHardwareRayTracing(
	TEXT("r.Lumen.TranslucencyVolume.HardwareRayTracing"),
	1,
	TEXT("Enables hardware ray tracing for Lumen translucency volume (Default = 1)"),
	ECVF_RenderThreadSafe
);

#endif // RHI_RAYTRACING

namespace Lumen
{
	bool UseHardwareRayTracedTranslucencyVolume(const FSceneViewFamily& ViewFamily)
	{
#if RHI_RAYTRACING
		return IsRayTracingEnabled()
			&& Lumen::UseHardwareRayTracing(ViewFamily)
			&& (CVarLumenTranslucencyVolumeHardwareRayTracing.GetValueOnRenderThread() != 0);
#else
		return false;
#endif
	}
}

#if RHI_RAYTRACING

class FLumenTranslucencyVolumeHardwareRayTracing : public FLumenHardwareRayTracingShaderBase
{
	DECLARE_LUMEN_RAYTRACING_SHADER(FLumenTranslucencyVolumeHardwareRayTracing, Lumen::ERayTracingShaderDispatchSize::DispatchSize1D)

	class FProbeSourceMode : SHADER_PERMUTATION_RANGE_INT("PROBE_SOURCE_MODE", 0, 3);
	using FPermutationDomain = TShaderPermutationDomain<FLumenHardwareRayTracingShaderBase::FBasePermutationDomain, FProbeSourceMode>;

	// Parameters
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D<float3>, RWVolumeTraceRadiance)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D<float>, RWVolumeTraceHitDistance)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float3>, VolumeFroxelProbeRadiance)
		SHADER_PARAMETER_STRUCT_INCLUDE(FLumenHardwareRayTracingShaderBase::FSharedParameters, SharedParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(LumenRadianceCache::FRadianceCacheInterpolationParameters, RadianceCacheParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(FLumenTranslucencyLightingVolumeParameters, VolumeParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(FLumenTranslucencyLightingVolumeTraceSetupParameters, TraceSetupParameters)
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

IMPLEMENT_LUMEN_RAYGEN_AND_COMPUTE_RAYTRACING_SHADERS(FLumenTranslucencyVolumeHardwareRayTracing)

IMPLEMENT_GLOBAL_SHADER(FLumenTranslucencyVolumeHardwareRayTracingRGS, "/Engine/Private/Lumen/LumenTranslucencyVolumeHardwareRayTracing.usf", "LumenTranslucencyVolumeHardwareRayTracingRGS", SF_RayGen);
IMPLEMENT_GLOBAL_SHADER(FLumenTranslucencyVolumeHardwareRayTracingCS, "/Engine/Private/Lumen/LumenTranslucencyVolumeHardwareRayTracing.usf", "LumenTranslucencyVolumeHardwareRayTracingCS", SF_Compute);


class FTranslucencyVolumeTraceFroxelProbesRayTracing : public FLumenHardwareRayTracingShaderBase
{
	DECLARE_LUMEN_RAYTRACING_SHADER(FTranslucencyVolumeTraceFroxelProbesRayTracing, Lumen::ERayTracingShaderDispatchSize::DispatchSize1D)

	class FDynamicSkyLight : SHADER_PERMUTATION_BOOL("ENABLE_DYNAMIC_SKY_LIGHT");
	using FPermutationDomain = TShaderPermutationDomain<FLumenHardwareRayTracingShaderBase::FBasePermutationDomain, FDynamicSkyLight>;

	// Parameters
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D<float3>, RWVolumeTraceRadiance)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D<float>, RWVolumeTraceHitDistance)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float3>, VolumeFroxelProbeRadiance)
		SHADER_PARAMETER_STRUCT_INCLUDE(FLumenHardwareRayTracingShaderBase::FSharedParameters, SharedParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(FLumenTranslucencyLightingVolumeParameters, VolumeParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(FLumenTranslucencyLightingVolumeTraceSetupParameters, TraceSetupParameters)
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

IMPLEMENT_LUMEN_RAYGEN_AND_COMPUTE_RAYTRACING_SHADERS(FTranslucencyVolumeTraceFroxelProbesRayTracing)

IMPLEMENT_GLOBAL_SHADER(FTranslucencyVolumeTraceFroxelProbesRayTracingRGS, "/Engine/Private/Lumen/LumenTranslucencyVolumeLighting.usf", "TranslucencyVolumeFroxelProbesRayTracingRGS", SF_RayGen);
IMPLEMENT_GLOBAL_SHADER(FTranslucencyVolumeTraceFroxelProbesRayTracingCS,  "/Engine/Private/Lumen/LumenTranslucencyVolumeLighting.usf", "TranslucencyVolumeFroxelProbesRayTracingCS", SF_Compute);


void FDeferredShadingSceneRenderer::PrepareLumenHardwareRayTracingTranslucencyVolumeLumenMaterial(const FViewInfo& View, TArray<FRHIRayTracingShader*>& OutRayGenShaders)
{
	if (Lumen::UseHardwareRayTracedTranslucencyVolume(*View.Family) && !Lumen::UseHardwareInlineRayTracing(*View.Family))
	{
		for (int32 UseFroxelProbes = 0; UseFroxelProbes < 2; ++UseFroxelProbes)
		{
			for (int32 VolumeRadianceCache = 0; VolumeRadianceCache < 2; ++VolumeRadianceCache)
			{
				FLumenTranslucencyVolumeHardwareRayTracingRGS::FPermutationDomain PermutationVector;
				PermutationVector.Set<FLumenTranslucencyVolumeHardwareRayTracingRGS::FProbeSourceMode>(UseFroxelProbes > 0 ? 2 : (VolumeRadianceCache > 0 ? 1 : 0));

				TShaderRef<FLumenTranslucencyVolumeHardwareRayTracingRGS> RayGenerationShader = View.ShaderMap->GetShader<FLumenTranslucencyVolumeHardwareRayTracingRGS>(PermutationVector);

				OutRayGenShaders.Add(RayGenerationShader.GetRayTracingShader());
			}
		}

		for (int32 DynamicSkyLight = 0; DynamicSkyLight < 2; ++DynamicSkyLight)
		{
			FTranslucencyVolumeTraceFroxelProbesRayTracingRGS::FPermutationDomain PermutationVector;
			PermutationVector.Set<FTranslucencyVolumeTraceFroxelProbesRayTracingRGS::FDynamicSkyLight>(DynamicSkyLight > 0);

			TShaderRef<FTranslucencyVolumeTraceFroxelProbesRayTracingRGS> RayGenerationShader = View.ShaderMap->GetShader<FTranslucencyVolumeTraceFroxelProbesRayTracingRGS>(PermutationVector);

			OutRayGenShaders.Add(RayGenerationShader.GetRayTracingShader());
		}
	}
}

#endif // RHI_RAYTRACING

void HardwareRayTraceTranslucencyVolume(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View,
	const FLumenCardTracingParameters& TracingParameters,
	LumenRadianceCache::FRadianceCacheInterpolationParameters RadianceCacheParameters,
	FLumenTranslucencyLightingVolumeParameters VolumeParameters,
	FLumenTranslucencyLightingVolumeTraceSetupParameters TraceSetupParameters,
	FRDGTextureRef VolumeTraceRadiance,
	FRDGTextureRef VolumeTraceHitDistance,
	FRDGTextureRef VolumeFroxelProbeRadiance,
	ERDGPassFlags ComputePassFlags
)
{
#if RHI_RAYTRACING
	bool bUseMinimalPayload = true;
	bool bInlineRayTracing = Lumen::UseHardwareInlineRayTracing(*View.Family);

	checkf(ComputePassFlags != ERDGPassFlags::AsyncCompute || bInlineRayTracing, TEXT("Async Lumen HWRT is only supported for inline ray tracing"));

	// Cast rays
	{
		FLumenTranslucencyVolumeHardwareRayTracingRGS::FParameters* PassParameters = GraphBuilder.AllocParameters<FLumenTranslucencyVolumeHardwareRayTracingRGS::FParameters>();

		SetLumenHardwareRayTracingSharedParameters(
			GraphBuilder,
			GetSceneTextureParameters(GraphBuilder, View),
			View,
			TracingParameters,
			&PassParameters->SharedParameters);

		PassParameters->RWVolumeTraceRadiance = GraphBuilder.CreateUAV(VolumeTraceRadiance);
		PassParameters->RWVolumeTraceHitDistance = GraphBuilder.CreateUAV(VolumeTraceHitDistance);
		PassParameters->VolumeFroxelProbeRadiance = VolumeFroxelProbeRadiance;
		PassParameters->VolumeParameters = VolumeParameters;
		PassParameters->TraceSetupParameters = TraceSetupParameters;
		PassParameters->RadianceCacheParameters = RadianceCacheParameters;

		FLumenTranslucencyVolumeHardwareRayTracingRGS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FLumenTranslucencyVolumeHardwareRayTracingRGS::FProbeSourceMode>(VolumeFroxelProbeRadiance != nullptr ? 2 : (RadianceCacheParameters.RadianceProbeIndirectionTexture != nullptr ? 1 : 0));

		const FIntPoint DispatchResolution(VolumeTraceRadiance->Desc.Extent * FIntPoint(VolumeTraceRadiance->Desc.Depth, 1));

		if (bInlineRayTracing)
		{
			const FIntVector GroupCount = FComputeShaderUtils::GetGroupCount(DispatchResolution, FLumenTranslucencyVolumeHardwareRayTracingCS::GetThreadGroupSize(View.GetShaderPlatform()));
			FLumenTranslucencyVolumeHardwareRayTracingCS::AddLumenRayTracingDispatch(
				GraphBuilder,
				RDG_EVENT_NAME("HardwareRayTracing (inline) %ux%u", DispatchResolution.X, DispatchResolution.Y),
				View, 
				PermutationVector,
				PassParameters,
				GroupCount,
				ComputePassFlags);
		}
		else
		{
			FLumenTranslucencyVolumeHardwareRayTracingRGS::AddLumenRayTracingDispatch(
				GraphBuilder,
				RDG_EVENT_NAME("HardwareRayTracing (raygen) %ux%u", DispatchResolution.X, DispatchResolution.Y),
				View, 
				PermutationVector,
				PassParameters,
				DispatchResolution,
				bUseMinimalPayload);
		}
	}

#else
	unimplemented();
#endif // RHI_RAYTRACING
}


void HardwareRayTraceTranslucencyVolumeFroxelProbes(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View,
	const FLumenCardTracingParameters& TracingParameters,
	FLumenTranslucencyLightingVolumeParameters VolumeParameters,
	FLumenTranslucencyLightingVolumeTraceSetupParameters TraceSetupParameters,
	FRDGTextureRef VolumeFroxelProbeRadiance,
	FRDGTextureRef VolumeFroxelProbeHitDistance,
	ERDGPassFlags ComputePassFlags,
	const bool bDynamicSkyLight
)
{
#if RHI_RAYTRACING
	bool bUseMinimalPayload = true;
	bool bInlineRayTracing = Lumen::UseHardwareInlineRayTracing(*View.Family);

	checkf(ComputePassFlags != ERDGPassFlags::AsyncCompute || bInlineRayTracing, TEXT("Async Lumen HWRT is only supported for inline ray tracing"));

	// Cast rays
	{
		FTranslucencyVolumeTraceFroxelProbesRayTracingRGS::FParameters* PassParameters = GraphBuilder.AllocParameters<FTranslucencyVolumeTraceFroxelProbesRayTracingRGS::FParameters>();

		SetLumenHardwareRayTracingSharedParameters(
			GraphBuilder,
			GetSceneTextureParameters(GraphBuilder, View),
			View,
			TracingParameters,
			&PassParameters->SharedParameters);

		PassParameters->RWVolumeTraceRadiance = GraphBuilder.CreateUAV(VolumeFroxelProbeRadiance);
		PassParameters->RWVolumeTraceHitDistance = GraphBuilder.CreateUAV(VolumeFroxelProbeHitDistance);
		PassParameters->VolumeFroxelProbeRadiance = VolumeFroxelProbeRadiance;
		PassParameters->VolumeParameters = VolumeParameters;
		PassParameters->TraceSetupParameters = TraceSetupParameters;

		FTranslucencyVolumeTraceFroxelProbesRayTracingRGS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FTranslucencyVolumeTraceFroxelProbesRayTracingRGS::FDynamicSkyLight>(bDynamicSkyLight);

		const FIntPoint DispatchResolution(VolumeFroxelProbeRadiance->Desc.Extent * FIntPoint(VolumeFroxelProbeRadiance->Desc.Depth, 1));

		if (bInlineRayTracing)
		{
			const FIntVector GroupCount = FComputeShaderUtils::GetGroupCount(DispatchResolution, FTranslucencyVolumeTraceFroxelProbesRayTracingCS::GetThreadGroupSize(View.GetShaderPlatform()));
			FTranslucencyVolumeTraceFroxelProbesRayTracingCS::AddLumenRayTracingDispatch(
				GraphBuilder,
				RDG_EVENT_NAME("HardwareRayTracing FroxelProbes (inline) %ux%u", DispatchResolution.X, DispatchResolution.Y),
				View,
				PermutationVector,
				PassParameters,
				GroupCount,
				ComputePassFlags);
		}
		else
		{
			FTranslucencyVolumeTraceFroxelProbesRayTracingRGS::AddLumenRayTracingDispatch(
				GraphBuilder,
				RDG_EVENT_NAME("HardwareRayTracing FroxelProbes (raygen) %ux%u", DispatchResolution.X, DispatchResolution.Y),
				View,
				PermutationVector,
				PassParameters,
				DispatchResolution,
				bUseMinimalPayload);
		}
	}

#else
	unimplemented();
#endif // RHI_RAYTRACING
}
