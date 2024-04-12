// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "RHIShaderPlatform.h"
#include "ShaderCompilerJobTypes.h"

struct FShaderDiagnosticInfo
{
	TArray<FShaderCommonCompileJob*> ErrorJobs;
	TArray<FString> UniqueErrors;
	TArray<FString> UniqueErrorPrefixes;
	TArray<FString> UniqueWarnings;
	TArray<EShaderPlatform> ErrorPlatforms;
	FString TargetShaderPlatformString;

	RENDERCORE_API FShaderDiagnosticInfo(const TArray<FShaderCommonCompileJobPtr>& Jobs);

private:
	void AddAndProcessErrorsForJob(FShaderCommonCompileJob& Job);
	void AddWarningsForJob(const FShaderCommonCompileJob& Job);
};

RENDERCORE_API FString GetSingleJobCompilationDump(const FShaderCompileJob* SingleJob);
RENDERCORE_API bool IsShaderDevelopmentModeEnabled();

