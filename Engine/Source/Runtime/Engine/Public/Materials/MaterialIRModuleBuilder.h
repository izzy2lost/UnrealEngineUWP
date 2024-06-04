// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Materials/MaterialIRCommon.h"

#if WITH_EDITOR

//
class FMaterialIRModuleBuilder
{
public:
	//
	void SetSource(FMaterial* InMaterial, const FStaticParameterSet* InStaticParameters);

	//
	void SetPlatform(EShaderPlatform InShaderPlatform, ERHIFeatureLevel::Type InFeatureLevel, const ITargetPlatform* InTargetPlatform);

	//
	void SetTarget(FMaterialIRModule* InModule);

	//
	bool Build();

private:
	FMaterial* Material;
	const FStaticParameterSet* StaticParameters;
	EShaderPlatform ShaderPlatform{};
	ERHIFeatureLevel::Type FeatureLevel{};
	const ITargetPlatform* TargetPlatform{};
	FMaterialIRModule* Module;

	TArray<FExpressionInput*> ExpressionAnalysisStack;

	struct FHelper;
	friend FHelper;
};

#endif
