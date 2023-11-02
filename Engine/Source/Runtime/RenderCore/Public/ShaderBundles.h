// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GlobalShader.h"
#include "HAL/Platform.h"
#include "Misc/AssertionMacros.h"
#include "RHICommandList.h"
#include "RHIDefinitions.h"
#include "Serialization/MemoryLayout.h"
#include "Shader.h"
#include "ShaderCore.h"
#include "ShaderParameterMacros.h"
#include "ShaderParameterStruct.h"
#include "ShaderParameterUtils.h"
#include "ShaderParameters.h"
#include "ShaderPermutation.h"

class FRHIShaderBundle;

class FDispatchShaderBundleCS : public FGlobalShader
{
	DECLARE_EXPORTED_GLOBAL_SHADER(FDispatchShaderBundleCS, RENDERCORE_API);

public:
	SHADER_USE_PARAMETER_STRUCT(FDispatchShaderBundleCS, FGlobalShader)

	static const uint32 ThreadGroupSizeX = 64;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, RENDERCORE_API)
		SHADER_PARAMETER(uint32, RecordCount)
		SHADER_PARAMETER(FUintVector4, PlatformData)
		SHADER_PARAMETER_SRV(ByteAddressBuffer, RecordArgBuffer)
		SHADER_PARAMETER_SRV(ByteAddressBuffer, RecordDataBuffer)
		SHADER_PARAMETER_UAV(RWByteAddressBuffer, RWExecutionBuffer)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment);
};

class FDispatchShaderBundle
{
public:
	static RENDERCORE_API void Dispatch(
		FRHIShaderBundle* ShaderBundle,
		uint32 RecordCount,
		FRHIComputeCommandList& RHICmdList,
		FRHIShaderResourceView* RecordArgBufferSRV,
		FRHIShaderResourceView* RecordDataBufferSRV,
		FRHIUnorderedAccessView* ExecutionBufferUAV
	);
};