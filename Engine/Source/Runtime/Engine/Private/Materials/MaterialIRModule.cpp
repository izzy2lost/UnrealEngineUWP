// Copyright Epic Games, Inc. All Rights Reserved.

#include "Materials/MaterialIRModule.h"
#include "Materials/MaterialIR.h"

#if WITH_EDITOR

namespace IR = UE::MIR;

FMaterialIRModule::FMaterialIRModule()
{
	RootBlock = new IR::FBlock;
}

FMaterialIRModule::~FMaterialIRModule()
{
	Empty();

	delete RootBlock;
}

void FMaterialIRModule::Empty()
{
	RootBlock->Instructions = nullptr;

	for (IR::FValue* Value : Values)
	{
		delete Value;
	}

	Values.Empty();
	Outputs.Empty();
}

#endif // #if WITH_EDITOR
