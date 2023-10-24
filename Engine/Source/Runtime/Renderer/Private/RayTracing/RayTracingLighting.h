// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ShaderParameters.h"
#include "ShaderParameterMacros.h"
#include "SceneRendering.h"
#include "LightSceneInfo.h"
#include "RayTracingDefinitions.h"
#include "RayTracingTypes.h"
#include "Containers/DynamicRHIResourceArray.h"

#if RHI_RAYTRACING

FRHIRayTracingShader* GetRayTracingLightingMissShader(const FGlobalShaderMap* ShaderMap);

BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FRaytracingLightDataPacked, RENDERER_API)
	SHADER_PARAMETER(uint32, Count)
	SHADER_PARAMETER(uint32, CellCount)
	SHADER_PARAMETER(float, CellScale)
	SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FRTLightingData>, LightDataBuffer)
	SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, LightIndices)
	SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint4>, LightCullingVolume)
END_GLOBAL_SHADER_PARAMETER_STRUCT()

using FRayTracingLightFunctionMap = TMap<const FLightSceneInfo*, int32>;

// Register this in the graph builder so we can easily move it around and access it from both the main rendering thread and RDG passes
RDG_REGISTER_BLACKBOARD_STRUCT(FRayTracingLightFunctionMap)

FRayTracingLightFunctionMap GatherLightFunctionLights(FScene* Scene, const FEngineShowFlags EngineShowFlags, ERHIFeatureLevel::Type InFeatureLevel);
FRayTracingLightFunctionMap GatherLightFunctionLightsPathTracing(FScene* Scene, const FEngineShowFlags EngineShowFlags, ERHIFeatureLevel::Type InFeatureLevel);

TRDGUniformBufferRef<FRaytracingLightDataPacked> CreateRayTracingLightData(
	FRDGBuilder& GraphBuilder,
	const FScene* Scene,
	const FSceneView& View,
	FGlobalShaderMap* ShaderMap,
	uint32& NumOfSkippedRayTracingLights);

void BindLightFunctionShaders(
	FRHICommandList& RHICmdList,
	const FScene* Scene,
	const FRayTracingLightFunctionMap* RayTracingLightFunctionMap,
	const class FViewInfo& View);

void BindLightFunctionShadersPathTracing(
	FRHICommandList& RHICmdList,
	const FScene* Scene,
	const FRayTracingLightFunctionMap* RayTracingLightFunctionMap,
	const class FViewInfo& View);

#endif
