// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Materials/MaterialIRCommon.h"

#if WITH_EDITOR

//
class FMaterialIRModuleBuilder
{
public:
	bool Build(UMaterial* InMaterial, FMaterialIRModule* TargetModule);

private:
	UMaterial* BaseMaterial;
	FMaterialIRModule* Module;
	TArray<UMaterialExpression*> ExpressionAnalysisStack;
	TMap<const FExpressionInput*, UE::MIR::FValuePtr> InputValues;
	TMap<const FExpressionOutput*, UE::MIR::FValuePtr> OutputValues;

	friend class UE::MIR::FEmitter;
	struct FPrivate;
};

#endif
