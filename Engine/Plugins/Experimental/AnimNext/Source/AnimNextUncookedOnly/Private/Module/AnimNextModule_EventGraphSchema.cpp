// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNextEventGraphSchema.h"

#include "AnimNextExecuteContext.h"
#include "Graph/RigUnit_AnimNextBase.h"
#include "Graph/RigUnit_AnimNextTraitStack.h"

bool UAnimNextEventGraphSchema::SupportsUnitFunction(URigVMController* InController, const FRigVMFunction* InUnitFunction) const
{
	if(const UScriptStruct* FunctionExecuteContextStruct = InUnitFunction->GetExecuteContextStruct())
	{
		if(FunctionExecuteContextStruct == FAnimNextExecuteContext::StaticStruct())
		{
			if(InUnitFunction->Struct)
			{
				if(InUnitFunction->Struct->IsChildOf(FRigUnit_AnimNextTraitStack::StaticStruct()))
				{
					return false;
				}
				return InUnitFunction->Struct->IsChildOf(FRigUnit_AnimNextBase::StaticStruct());
			}
		}
	}

	return Super::SupportsUnitFunction(InController, InUnitFunction);
}
