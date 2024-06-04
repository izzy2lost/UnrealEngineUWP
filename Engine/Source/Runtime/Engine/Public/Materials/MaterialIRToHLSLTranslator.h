// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Materials/MaterialIRCommon.h"

#if WITH_EDITOR

class FMaterialIRToHLSLTranslator
{
public:
	using FParametersMap = TMap<FString, FString>;

public:
	FMaterialIRToHLSLTranslator();
	void Translate(const FMaterial& InMaterial, const FMaterialIRModule& InModule, FParametersMap& OutParameters, FShaderCompilerEnvironment& OutEnvironment);

private:
 	
	UE_MIR_PRIVATE();
};

#endif // #if WITH_EDITOR
