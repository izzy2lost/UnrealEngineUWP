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

	FDispatchShaderBundleCS() = default;
	FDispatchShaderBundleCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		RecordCountParam.Bind(Initializer.ParameterMap, TEXT("RecordCount"), SPF_Mandatory);
		PlatformDataParam.Bind(Initializer.ParameterMap, TEXT("PlatformData"), SPF_Mandatory);
		RecordArgBufferParam.Bind(Initializer.ParameterMap, TEXT("RecordArgBuffer"), SPF_Mandatory);
		RecordDataBufferParam.Bind(Initializer.ParameterMap, TEXT("RecordDataBuffer"), SPF_Mandatory);
		RWExecutionBufferParam.Bind(Initializer.ParameterMap, TEXT("RWExecutionBuffer"), SPF_Mandatory);
	}

	static const uint32 ThreadGroupSizeX = 64;

	LAYOUT_FIELD(FShaderParameter, RecordCountParam);
	LAYOUT_FIELD(FShaderParameter, PlatformDataParam);
	LAYOUT_FIELD(FShaderResourceParameter, RecordArgBufferParam);
	LAYOUT_FIELD(FShaderResourceParameter, RecordDataBufferParam);
	LAYOUT_FIELD(FShaderResourceParameter, RWExecutionBufferParam);

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