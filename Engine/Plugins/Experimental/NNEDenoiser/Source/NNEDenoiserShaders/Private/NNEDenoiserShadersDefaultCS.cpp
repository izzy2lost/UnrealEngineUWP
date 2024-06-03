// Copyright Epic Games, Inc. All Rights Reserved.

#include "NNEDenoiserShadersDefaultCS.h"

namespace UE::NNEDenoiserShaders::Internal
{
	void CommonModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& InParameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(InParameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREAD_GROUP_SIZE"), FNNEDenoiserConstants::THREAD_GROUP_SIZE);
		OutEnvironment.SetDefine(TEXT("MAX_NUM_MAPPED_CHANNELS"), FNNEDenoiserConstants::MAX_NUM_MAPPED_CHANNELS);
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

	IMPLEMENT_GLOBAL_SHADER(FNNEDenoiserTextureBufferMappedCopyCS, "/NNEDenoiserShaders/NNEDenoiserShadersDefault.usf", "MappedCopy", SF_Compute);
	IMPLEMENT_GLOBAL_SHADER(FNNEDenoiserBufferMappedCopyCS, "/NNEDenoiserShaders/NNEDenoiserShadersDefault.usf", "MappedCopy", SF_Compute);

	IMPLEMENT_GLOBAL_SHADER(FNNEDenoiserBufferTextureMappedCopyCS, "/NNEDenoiserShaders/NNEDenoiserShadersDefault.usf", "MappedCopy", SF_Compute);
	IMPLEMENT_GLOBAL_SHADER(FNNEDenoiserTextureMappedCopyCS, "/NNEDenoiserShaders/NNEDenoiserShadersDefault.usf", "MappedCopy", SF_Compute);

} // UE::NNEDenoiser::Private