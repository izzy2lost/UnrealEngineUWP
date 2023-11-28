// Copyright Epic Games, Inc. All Rights Reserved.

#include "NNEHlslShadersSoftmaxCS.h"
#include "NNE.h"

namespace UE::NNEHlslShaders::Internal
{
	FIntVector TSoftmaxCS::GetGroupCount(const TSoftmaxCS::FParameters& Parameters)
	{
		return { 1, (int32)Parameters.N, 1 };
	}

	void TSoftmaxCS::ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& InParameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(InParameters, OutEnvironment);

		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE_X"), FSoftmaxConstants::NUM_GROUP_THREADS);
	}

	IMPLEMENT_GLOBAL_SHADER(TSoftmaxCS, "/NNE/NNEHlslShadersSoftmax.usf", "Softmax", SF_Compute);
} // UE::NNEHlslShaders::Internal
