// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Materials/MaterialIRCommon.h"
#include "MaterialShared.h"

#if WITH_EDITOR

class FMaterialIRModule
{
public:
	struct FError
	{
		UMaterialExpression* Expression;
		FString Message;
	};

public:
	~FMaterialIRModule();
	void Empty();
	const FMaterialCompilationOutput& GetCompilationOutput() const { return CompilationOutput; }
	TArrayView<const UE::MIR::FSetMaterialOutput* const> GetOutputs() const { return Outputs; }
	TArrayView<const FError> GetErrors() const { return Errors; }

private:
	FMaterialCompilationOutput CompilationOutput;
	TArray<UE::MIR::FValuePtr> Values;
	TArray<UE::MIR::FSetMaterialOutput*> Outputs;
	TArray<FError> Errors;


	friend UE::MIR::FEmitter;
	friend FMaterialIRModuleBuilder;
};

#endif // #if WITH_EDITOR
