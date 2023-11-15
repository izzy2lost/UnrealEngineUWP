// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RendererPrivate.h"
#include "BlueNoise.h"

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
	SHADER_PARAMETER(FIntPoint, ShadowMaskPageTablePerLightSize)
	SHADER_PARAMETER(FIntPoint, ShadingTileGridSize)
	SHADER_PARAMETER(FVector2f, DownsampledBufferInvSize)
	SHADER_PARAMETER(float, SamplingMinWeight)
	SHADER_PARAMETER(int32, TileDataStride)
	SHADER_PARAMETER(int32, DownsampledTileDataStride)
	SHADER_PARAMETER(int32, TemporalMaxFramesAccumulated)
	SHADER_PARAMETER(float, TemporalStdDevOffset)
	SHADER_PARAMETER(int32, DebugMode)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float>, DownsampledSceneDepth)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D<UNORM float3>, DownsampledSceneWorldNormal)
END_SHADER_PARAMETER_STRUCT()

// Internal functions, don't use outside of the StochasticShadows
namespace StochasticShadows
{
	void RayTraceLightSamples(
		const FViewInfo& View,
		FRDGBuilder& GraphBuilder,
		const FSceneTextures& SceneTextures,
		const FIntPoint SampleBufferSize,
		FRDGTextureRef LightSamples,
		FRDGTextureRef LightSampleRayDistance,
		const FStochasticShadowsParameters& StochasticShadowsParameters
	);

	bool ShouldCompileShaders(const FGlobalShaderPermutationParameters& Parameters);
	bool UseWaveOps(EShaderPlatform ShaderPlatform);
	int32 GetDebugMode();
	uint32 GetNumSamplesPerPixel();
	FIntPoint GetNumSamplesPerPixel2d(uint32 NumSamplesPerPixel1d);
};