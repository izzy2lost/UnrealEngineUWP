// Copyright Epic Games, Inc. All Rights Reserved.

#include "ShaderBundles.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "RenderGraph.h"
#include "RenderGraphDefinitions.h"
#include "ShaderParameterMacros.h"
#include "RenderGraphFwd.h"
#include "ShaderCompilerCore.h"

IMPLEMENT_GLOBAL_SHADER(FDispatchShaderBundleCS, "/Engine/Private/ShaderBundleDispatch.usf", "DispatchShaderBundleEntry", SF_Compute);

bool FDispatchShaderBundleCS::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
{
	return RHISupportsShaderBundleDispatch(Parameters.Platform);
}

void FDispatchShaderBundleCS::ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
{
	if (FDataDrivenShaderPlatformInfo::GetRequiresBindfulUtilityShaders(Parameters.Platform))
	{
		OutEnvironment.CompilerFlags.Add(CFLAG_ForceBindful);
	}

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

	FRHIBatchedShaderParameters& Parameters = RHICmdList.GetScratchShaderParameters();

	SetShaderValue(Parameters, ComputeShader->RecordCountParam, RecordCount);
	SetShaderValue(Parameters, ComputeShader->PlatformDataParam, ShaderBundle->GetPlatformData());
	SetSRVParameter(Parameters, ComputeShader->RecordArgBufferParam, RecordArgBufferSRV);
	SetSRVParameter(Parameters, ComputeShader->RecordDataBufferParam, RecordDataBufferSRV);
	SetUAVParameter(Parameters, ComputeShader->RWExecutionBufferParam, ExecutionBufferUAV);

	FRHIComputeShader* ComputeShaderRHI = ComputeShader.GetComputeShader();
	SetComputePipelineState(RHICmdList, ComputeShaderRHI);

	RHICmdList.SetBatchedShaderParameters(ComputeShaderRHI, Parameters);

	const FIntVector GroupCount = FComputeShaderUtils::GetGroupCount(RecordCount, FDispatchShaderBundleCS::ThreadGroupSizeX);
	DispatchComputeShader(RHICmdList, ComputeShader, GroupCount.X, GroupCount.Y, GroupCount.Z);

	FRHIBatchedShaderUnbinds& Unbinds = RHICmdList.GetScratchShaderUnbinds();

	UnsetUAVParameter(Unbinds, ComputeShader->RWExecutionBufferParam);
	RHICmdList.SetBatchedShaderUnbinds(ComputeShaderRHI, Unbinds);
}