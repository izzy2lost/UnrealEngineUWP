// Copyright Epic Games, Inc. All Rights Reserved.

#include "Materials/MaterialIRModule.h"
#include "Materials/MaterialIR.h"

#if WITH_EDITOR

namespace MIR = MaterialIR;

FMaterialIRModule::~FMaterialIRModule()
{
	Empty();
}

void FMaterialIRModule::Empty()
{
	for (MIR::FValue* Value : Values)
	{
		delete Value;
	}

	Values.Empty();
	Outputs.Empty();
}

void FMaterialIRModule::Reset(EShaderPlatform InShaderPlatform, ERHIFeatureLevel::Type InFeatureLevel, const ITargetPlatform* InTargetPlatform)
{
	Empty();

	ShaderPlatform = InShaderPlatform;
	TargetPlatform = InTargetPlatform;
}

#endif
