// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/AnimNextGraph_EventGraphSchema.h"

#include "AnimNextExecuteContext.h"
#include "Param/RigUnit_AnimNextParameterBase.h"

bool UAnimNextGraph_EventGraphSchema::SupportsUnitFunction(URigVMController* InController, const FRigVMFunction* InUnitFunction) const
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
