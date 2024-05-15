// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Materials/MaterialIRCommon.h"
#include "MaterialShared.h"

#if WITH_EDITOR

class FMaterialIRModule
{
public:
	~FMaterialIRModule();
	void Empty();
	void Reset(EShaderPlatform InShaderPlatform, ERHIFeatureLevel::Type InFeatureLevel, const ITargetPlatform* InTargetPlatform);
	EShaderPlatform GetShaderPlatform() const { return ShaderPlatform; }
	const ITargetPlatform* GetTargetPlatform() const { return TargetPlatform; }
	const FMaterialCompilationOutput& GetCompilationOutput() const { return CompilationOutput; }
	TArrayView<const MaterialIR::FSetMaterialOutputInstr* const> GetOutputs() const { return Outputs; }

private:
	EShaderPlatform ShaderPlatform;
	const ITargetPlatform* TargetPlatform;
	FMaterialCompilationOutput CompilationOutput;
	TArray<MaterialIR::FValue*> Values;
	TArray<MaterialIR::FSetMaterialOutputInstr*> Outputs;

	friend MaterialIR::FBuilder;
};

#endif
