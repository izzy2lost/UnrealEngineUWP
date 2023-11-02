// Copyright Epic Games, Inc. All Rights Reserved.

#include "ShaderBundles.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "RenderGraph.h"
#include "RenderGraphDefinitions.h"
#include "ShaderParameterMacros.h"
#include "RenderGraphFwd.h"

IMPLEMENT_GLOBAL_SHADER(FDispatchShaderBundleCS, "/Engine/Private/ShaderBundleDispatch.usf", "DispatchShaderBundleEntry", SF_Compute);

bool FDispatchShaderBundleCS::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
{
	return RHISupportsShaderBundleDispatch(Parameters.Platform);
}

void FDispatchShaderBundleCS::ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
{
	OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEX"), ThreadGroupSizeX);
	OutEnvironment.SetDefine(TEXT("USE_SHADER_ROOT_CONSTANTS"), RHISupportsShaderRootConstants(Parameters.Platform) ? 1 : 0);
	FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
}

void FDispatchShaderBundle::Dispatch(
	FRHIShaderBundle* ShaderBundle,
	uint32 RecordCount,
	FRHIComputeCommandList& RHICmdList,
	FRHIShaderResourceView* RecordArgBufferSRV,
	FRHIShaderResourceView* RecordDataBufferSRV,
	FRHIUnorderedAccessView* ExecutionBufferUAV
)
{
	check(RHISupportsShaderBundleDispatch(GMaxRHIShaderPlatform) && GRHISupportsShaderBundleDispatch);
	check(ShaderBundle && ShaderBundle->NumRecords > 0 && RecordCount <= ShaderBundle->NumRecords);

	RHICmdList.ClearUAVUint(ExecutionBufferUAV, FUintVector4(0, 0, 0, 0));

	auto ComputeShader = GetGlobalShaderMap(GMaxRHIFeatureLevel)->GetShader<FDispatchShaderBundleCS>();

	FDispatchShaderBundleCS::FParameters Parameters;

	Parameters.RecordCount = RecordCount;
	Parameters.PlatformData = ShaderBundle->GetPlatformData();
	Parameters.RecordArgBuffer = RecordArgBufferSRV;
	Parameters.RecordDataBuffer = RecordDataBufferSRV;
	Parameters.RWExecutionBuffer = ExecutionBufferUAV;

	const FIntVector GroupCount = FComputeShaderUtils::GetGroupCount(RecordCount, FDispatchShaderBundleCS::ThreadGroupSizeX);

	FRHIComputeShader* ComputeShaderRHI = ComputeShader.GetComputeShader();
	SetComputePipelineState(RHICmdList, ComputeShaderRHI);
	SetShaderParameters(RHICmdList, ComputeShader, ComputeShaderRHI, Parameters);
	DispatchComputeShader(RHICmdList, ComputeShader, GroupCount.X, GroupCount.Y, GroupCount.Z);
	UnsetShaderUAVs(RHICmdList, ComputeShader, ComputeShaderRHI);
}