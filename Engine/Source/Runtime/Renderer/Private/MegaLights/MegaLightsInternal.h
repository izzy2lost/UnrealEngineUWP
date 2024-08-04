// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RendererPrivate.h"
#include "BlueNoise.h"

BEGIN_SHADER_PARAMETER_STRUCT(FMegaLightsParameters, )
	SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, ViewUniformBuffer)
	SHADER_PARAMETER_STRUCT_INCLUDE(ShaderPrint::FShaderParameters, ShaderPrintUniformBuffer)
	SHADER_PARAMETER_STRUCT_INCLUDE(FSceneTextureParameters, SceneTextures)
	SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSceneUniformParameters, Scene)
	SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSceneTextureUniformParameters, SceneTexturesStruct)
	SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSubstrateGlobalUniformParameters, Substrate)
	SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FForwardLightData, ForwardLightData)
	SHADER_PARAMETER_RDG_UNIFORM_BUFFER(LightFunctionAtlas::FLightFunctionAtlasGlobalParameters, LightFunctionAtlas)
	SHADER_PARAMETER_STRUCT_REF(FBlueNoise, BlueNoise)
	SHADER_PARAMETER_TEXTURE(Texture2D, PreIntegratedGF)
	SHADER_PARAMETER_SAMPLER(SamplerState, PreIntegratedGFSampler)
	SHADER_PARAMETER(FIntPoint, SampleViewMin)
	SHADER_PARAMETER(FIntPoint, SampleViewSize)
	SHADER_PARAMETER(FIntPoint, DownsampledViewMin)
	SHADER_PARAMETER(FIntPoint, DownsampledViewSize)
	SHADER_PARAMETER(FIntPoint, NumSamplesPerPixel)
	SHADER_PARAMETER(FIntPoint, NumSamplesPerPixelDivideShift)
	SHADER_PARAMETER(FVector2f, DownsampledBufferInvSize)
	SHADER_PARAMETER(uint32, DownsampleFactor)
	SHADER_PARAMETER(uint32, MegaLightsStateFrameIndex)
	SHADER_PARAMETER(float, SamplingMinWeight)
	SHADER_PARAMETER(int32, TileDataStride)
	SHADER_PARAMETER(int32, DownsampledTileDataStride)
	SHADER_PARAMETER(float, TemporalMaxFramesAccumulated)
	SHADER_PARAMETER(float, TemporalNeighborhoodClampScale)
	SHADER_PARAMETER(int32, TemporalAdvanceFrame)
	SHADER_PARAMETER(int32, DebugMode)
	SHADER_PARAMETER(int32, DebugLightId)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D<uint>, DownsampledTileMask)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float>, DownsampledSceneDepth)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D<UNORM float3>, DownsampledSceneWorldNormal)
END_SHADER_PARAMETER_STRUCT()

// Internal functions, don't use outside of the MegaLights
namespace MegaLights
{
	void RayTraceLightSamples(
		const FSceneViewFamily& ViewFamily,
		const FViewInfo& View,
		FRDGBuilder& GraphBuilder,
		const FSceneTextures& SceneTextures,
		const FIntPoint SampleBufferSize,
		FRDGTextureRef LightSamples,
		FRDGTextureRef LightSampleRayDistance,
		const FMegaLightsParameters& MegaLightsParameters
	);

	bool ShouldCompileShaders(const FGlobalShaderPermutationParameters& Parameters);
	bool UseWaveOps(EShaderPlatform ShaderPlatform);
	int32 GetDebugMode();

	void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment);
};