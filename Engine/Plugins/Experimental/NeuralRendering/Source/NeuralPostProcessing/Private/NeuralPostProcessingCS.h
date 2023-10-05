// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GlobalShader.h"
#include "ShaderParameterUtils.h"
#include "RenderGraphUtils.h"
#include "ScreenPass.h"

#define NEURAL_POST_PROCESSING_THREAD_GROUP_SIZE 32

BEGIN_SHADER_PARAMETER_STRUCT(FNueralPostProcessInput, )
	SHADER_PARAMETER_STRUCT_INCLUDE(FScreenPassTextureViewportParameters, Viewport)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, Texture)
END_SHADER_PARAMETER_STRUCT()

FNueralPostProcessInput GetNeuralPostProcessInput(FRDGTextureRef Texture, const FScreenPassTextureViewportParameters& ViewportParameters);

class FNeuralPostProcessingBuildIndirectDispatchArgsCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FNeuralPostProcessingBuildIndirectDispatchArgsCS);
	SHADER_USE_PARAMETER_STRUCT(FNeuralPostProcessingBuildIndirectDispatchArgsCS, FGlobalShader)

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWIndirectDispatchArgsBuffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, SourceType)
		SHADER_PARAMETER(FIntPoint, NetworkTextureSize)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);
};

class FNeuralPostProcessingPrepareInputCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FNeuralPostProcessingPrepareInputCS)
	SHADER_USE_PARAMETER_STRUCT(FNeuralPostProcessingPrepareInputCS, FGlobalShader)

public:

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT(FNueralPostProcessInput, Input0)
		SHADER_PARAMETER_SAMPLER(SamplerState, InputTextureSampler)
		RDG_BUFFER_ACCESS(IndirectDispatchArgsBuffer, ERHIAccess::IndirectArgs)
		SHADER_PARAMETER(int32, InputBufferWidth)
		SHADER_PARAMETER(int32, InputBufferHeight)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<float>, InputBuffer)
		SHADER_PARAMETER(float, ColorScale)
	END_SHADER_PARAMETER_STRUCT()

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& InParameters, FShaderCompilerEnvironment& OutEnvironment);
};

class FNeuralPostProcessingProcessOutputPS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FNeuralPostProcessingProcessOutputPS)
	SHADER_USE_PARAMETER_STRUCT(FNeuralPostProcessingProcessOutputPS, FGlobalShader)

public:

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT(FNueralPostProcessInput, Input0)
		SHADER_PARAMETER(int32, OutputBufferWidth)
		SHADER_PARAMETER(int32, OutputBufferHeight)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<float>, OutputBuffer)
		SHADER_PARAMETER(float, ColorScale)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& InParameters, FShaderCompilerEnvironment& OutEnvironment);
};
