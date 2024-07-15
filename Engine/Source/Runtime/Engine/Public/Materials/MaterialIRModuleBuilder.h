// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Materials/MaterialIRCommon.h"

#if WITH_EDITOR

struct FMaterialIRModuleBuildParams
{
	UMaterial* Material;
	EShaderPlatform ShaderPlatform;
	const ITargetPlatform* TargetPlatform;
	const FStaticParameterSet& StaticParameters;
	FMaterialInsights* TargetInsight{};
};

//
class FMaterialIRModuleBuilder
{
public:
	bool Build(const FMaterialIRModuleBuildParams& Params, FMaterialIRModule* TargetModule);

private:
	TMap<const FExpressionInput*, UE::MIR::FValue*> InputValues;
	TMap<const FExpressionOutput*, UE::MIR::FValue*> OutputValues;

	friend class UE::MIR::FEmitter;
	struct FPrivate;
};

#endif
