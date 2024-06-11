// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNextEventGraphSchema.h"

#include "AnimNextExecuteContext.h"
#include "Param/RigUnit_AnimNextParameterBase.h"

bool UAnimNextEventGraphSchema::SupportsUnitFunction(URigVMController* InController, const FRigVMFunction* InUnitFunction) const
{
	if(const UScriptStruct* FunctionExecuteContextStruct = InUnitFunction->GetExecuteContextStruct())
	{
		if(FunctionExecuteContextStruct == FAnimNextExecuteContext::StaticStruct())
		{
			// Only allow nodes that are children of FRigUnit_AnimNextParameterBase
			if(InUnitFunction->Struct)
			{
				return InUnitFunction->Struct->IsChildOf(FRigUnit_AnimNextParameterBase::StaticStruct());
			}
		}
	}

	return Super::SupportsUnitFunction(InController, InUnitFunction);
}
