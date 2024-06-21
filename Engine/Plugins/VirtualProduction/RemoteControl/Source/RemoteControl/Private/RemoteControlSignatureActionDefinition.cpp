// Copyright Epic Games, Inc. All Rights Reserved.

#include "RCSignatureActionDefinition.h"
#include "RCSignatureAction.h"
#include "UObject/StructOnScope.h"

FRCSignatureActionDefinition::FRCSignatureActionDefinition(const UScriptStruct* InScriptStruct, const FRCSignatureField& InFieldOwner)
{
	ActionInstance.InitializeAsScriptStruct(InScriptStruct, /*StructMemory*/nullptr);

	if (FRCSignatureAction* Action = ActionInstance.GetMutablePtr())
	{
		Action->Initialize(InFieldOwner);
	}
}

const FRCSignatureAction* FRCSignatureActionDefinition::GetAction() const
{
	return ActionInstance.GetPtr();
}

TSharedRef<FStructOnScope> FRCSignatureActionDefinition::MakeStructOnScope()
{
	return MakeShared<FStructOnScope>(ActionInstance.GetScriptStruct(), ActionInstance.GetMutableMemory());
}

bool FRCSignatureActionDefinition::Execute(const FRCSignatureActionContext& InContext) const
{
	if (const FRCSignatureAction* Action = ActionInstance.GetPtr())
	{
		return Action->Execute(InContext);
	}
	return false;
}
