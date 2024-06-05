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
	void SetTarget(EShaderPlatform InShaderPlatform, const ITargetPlatform* InTargetPlatform, ERHIFeatureLevel::Type InFeatureLevel);
	bool Translate(const FMaterial& InMaterial, const FStaticParameterSet& StaticParameters, const FMaterialIRModule& InModule, FParametersMap& OutParameters, FShaderCompilerEnvironment& OutEnvironment);

private:
	EShaderPlatform ShaderPlatform{};
	const ITargetPlatform* TargetPlatform{};
	ERHIFeatureLevel::Type FeatureLevel{};

	struct FPrivate;
};

#endif // #if WITH_EDITOR
