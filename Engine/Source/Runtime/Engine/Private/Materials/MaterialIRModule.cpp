// Copyright Epic Games, Inc. All Rights Reserved.

#include "Materials/MaterialIRModule.h"
#include "Materials/MaterialIR.h"

#if WITH_EDITOR

namespace IR = UE::MIR;

FMaterialIRModule::~FMaterialIRModule()
{
	Empty();
}

void FMaterialIRModule::Empty()
{
	for (IR::FValuePtr Value : Values)
	{
		delete Value;
	}

	Values.Empty();
	Outputs.Empty();
}

#endif // #if WITH_EDITOR
