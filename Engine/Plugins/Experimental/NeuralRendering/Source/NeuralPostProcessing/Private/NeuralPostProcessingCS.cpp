// Copyright Epic Games, Inc. All Rights Reserved.


#include "NeuralPostProcessingCS.h"


FNueralPostProcessInput GetNeuralPostProcessInput(FRDGTextureRef Texture, const FScreenPassTextureViewportParameters& ViewportParameters)
{
	FNueralPostProcessInput Input;
	Input.Texture = Texture;
	Input.Viewport = ViewportParameters;
	return Input;
}


void FNeuralPostProcessingBuildIndirectDispatchArgsCS::ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
{
	FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	OutEnvironment.SetDefine(TEXT("THREAD_GROUP_SIZE"), NEURAL_POST_PROCESSING_THREAD_GROUP_SIZE);
}

bool FNeuralPostProcessingBuildIndirectDispatchArgsCS::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
{
	return true;
}

void FNeuralPostProcessingPrepareInputCS::ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& InParameters, FShaderCompilerEnvironment& OutEnvironment)
{
	FGlobalShader::ModifyCompilationEnvironment(InParameters, OutEnvironment);
	OutEnvironment.SetDefine(TEXT("THREAD_GROUP_SIZE"), NEURAL_POST_PROCESSING_THREAD_GROUP_SIZE);
}

void FNeuralPostProcessingProcessOutputPS::ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& InParameters, FShaderCompilerEnvironment& OutEnvironment)
{
	FGlobalShader::ModifyCompilationEnvironment(InParameters, OutEnvironment);
	OutEnvironment.SetDefine(TEXT("THREAD_GROUP_SIZE"), NEURAL_POST_PROCESSING_THREAD_GROUP_SIZE);
}

IMPLEMENT_GLOBAL_SHADER(FNeuralPostProcessingBuildIndirectDispatchArgsCS, "/NeuralRendering/NeuralPostProcessing.usf", "BuildIndirectDispatchArgsCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FNeuralPostProcessingPrepareInputCS, "/NeuralRendering/NeuralPostProcessing.usf", "PrepareInput", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FNeuralPostProcessingProcessOutputPS, "/NeuralRendering/NeuralPostProcessing.usf", "ProcessOutput", SF_Pixel);