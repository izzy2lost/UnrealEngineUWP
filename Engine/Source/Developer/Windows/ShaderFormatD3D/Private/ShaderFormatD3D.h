// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Windows/WindowsHWrapper.h"
#include "ShaderCompilerCommon.h"

// Controls whether r.Shaders.RemoveDeadCode should be honored
#ifndef UE_D3D_SHADER_COMPILER_ALLOW_DEAD_CODE_REMOVAL
#define UE_D3D_SHADER_COMPILER_ALLOW_DEAD_CODE_REMOVAL 1
#endif // UE_D3D_SHADER_COMPILER_ALLOW_DEAD_CODE_REMOVAL

struct FShaderTarget;

enum class ED3DShaderModel
{
	Invalid,
	SM5_0,
	SM6_0,
	SM6_6,
};

inline bool DoesShaderModelRequireDXC(ED3DShaderModel ShaderModel)
{
	return ShaderModel >= ED3DShaderModel::SM6_0;
}

bool PreprocessD3DShader(
	const FShaderCompilerInput& Input,
	const FShaderCompilerEnvironment& MergedEnvironment,
	FShaderPreprocessOutput& PreprocessOutput);

void CompileD3DShader(
	const FShaderCompilerInput& Input,
	const FString& InPreprocessedSource,
	FShaderCompilerOutput& Output,
	const FString& WorkingDirectory,
	ED3DShaderModel ShaderModel);

/**
 * @param bSecondPassAferUnusedInputRemoval whether we're compiling the shader second time, after having removed the unused inputs discovered in the first pass
 */
bool CompileAndProcessD3DShaderFXC(
	const FShaderCompilerInput& Input,
	const FString& InPreprocessedSource,
	const FString& InEntryPointName,
	const FShaderParameterParser& ShaderParameterParser,
	const TCHAR* ShaderProfile,
	bool bSecondPassAferUnusedInputRemoval,
	FShaderCompilerOutput& Output);

bool CompileAndProcessD3DShaderDXC(
	const FShaderCompilerInput& Input,
	const FString& InPreprocessedSource,
	const FString& InEntryPointName,
	const FShaderParameterParser& ShaderParameterParser,
	const TCHAR* ShaderProfile,
	ED3DShaderModel ShaderModel,
	bool bProcessingSecondTime,
	FShaderCompilerOutput& Output);

bool ValidateResourceCounts(uint32 NumSRVs, uint32 NumSamplers, uint32 NumUAVs, uint32 NumCBs, TArray<FString>& OutFilteredErrors);
