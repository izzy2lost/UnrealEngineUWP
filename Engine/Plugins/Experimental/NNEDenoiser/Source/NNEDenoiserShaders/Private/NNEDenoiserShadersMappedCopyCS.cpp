// Copyright Epic Games, Inc. All Rights Reserved.

#include "NNEDenoiserShadersMappedCopyCS.h"

namespace UE::NNEDenoiserShaders::Internal
{
	void CommonModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& InParameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(InParameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREAD_GROUP_SIZE"), FMappedCopyConstants::THREAD_GROUP_SIZE);
		OutEnvironment.SetDefine(TEXT("MAX_NUM_MAPPED_CHANNELS"), FMappedCopyConstants::MAX_NUM_MAPPED_CHANNELS);
	}

	void FNNEDenoiserTextureBufferMappedCopyCS::ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& InParameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		CommonModifyCompilationEnvironment(InParameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("INPUT_TYPE"), 0);
		OutEnvironment.SetDefine(TEXT("OUTPUT_TYPE"), 1);
	}

	void FNNEDenoiserBufferMappedCopyCS::ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& InParameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		CommonModifyCompilationEnvironment(InParameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("INPUT_TYPE"), 1);
		OutEnvironment.SetDefine(TEXT("OUTPUT_TYPE"), 1);
	}

	void FNNEDenoiserBufferTextureMappedCopyCS::ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& InParameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		CommonModifyCompilationEnvironment(InParameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("INPUT_TYPE"), 1);
		OutEnvironment.SetDefine(TEXT("OUTPUT_TYPE"), 0);
	}

	void FNNEDenoiserTextureMappedCopyCS::ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& InParameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		CommonModifyCompilationEnvironment(InParameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("INPUT_TYPE"), 0);
		OutEnvironment.SetDefine(TEXT("OUTPUT_TYPE"), 0);
	}

	bool FNNEDenoiserTextureBufferMappedCopyCS::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return ShouldCompileRayTracingShadersForProject(Parameters.Platform);
	}

	bool FNNEDenoiserBufferMappedCopyCS::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return ShouldCompileRayTracingShadersForProject(Parameters.Platform);
	}

	bool FNNEDenoiserBufferTextureMappedCopyCS::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return ShouldCompileRayTracingShadersForProject(Parameters.Platform);
	}

	bool FNNEDenoiserTextureMappedCopyCS::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return ShouldCompileRayTracingShadersForProject(Parameters.Platform);
	}

	IMPLEMENT_GLOBAL_SHADER(FNNEDenoiserTextureBufferMappedCopyCS, "/NNEDenoiserShaders/NNEDenoiserShadersMappedCopy.usf", "MappedCopy", SF_Compute);
	IMPLEMENT_GLOBAL_SHADER(FNNEDenoiserBufferMappedCopyCS, "/NNEDenoiserShaders/NNEDenoiserShadersMappedCopy.usf", "MappedCopy", SF_Compute);

	IMPLEMENT_GLOBAL_SHADER(FNNEDenoiserBufferTextureMappedCopyCS, "/NNEDenoiserShaders/NNEDenoiserShadersMappedCopy.usf", "MappedCopy", SF_Compute);
	IMPLEMENT_GLOBAL_SHADER(FNNEDenoiserTextureMappedCopyCS, "/NNEDenoiserShaders/NNEDenoiserShadersMappedCopy.usf", "MappedCopy", SF_Compute);

} // UE::NNEDenoiser::Private