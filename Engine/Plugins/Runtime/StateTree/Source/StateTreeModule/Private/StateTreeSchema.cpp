// Copyright Epic Games, Inc. All Rights Reserved.

#include "StateTreeSchema.h"
#include "Blueprint/StateTreeNodeBlueprintBase.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(StateTreeSchema)

namespace StateTreeSchemaUtilityCVars
{
	FAutoConsoleVariable CVarAllowUtilityConsiderations
	(
		TEXT("StateTree.AllowUtilityConsiderations"),
		false,
		TEXT("Reveal Experimental Utility Consideration in the State Tree Editor."),
		ECVF_Default
	);
}

bool UStateTreeSchema::IsChildOfBlueprintBase(const UClass* InClass) const
{
	return InClass->IsChildOf<UStateTreeNodeBlueprintBase>();
}

