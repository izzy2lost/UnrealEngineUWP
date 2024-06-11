// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/AnimNextAnimationGraphSchema.h"

#include "AnimNextExecuteContext.h"
#include "Graph/RigUnit_AnimNextBase.h"
#include "Param/RigVMDispatch_GetParameter.h"
#include "Param/RigVMDispatch_GetScopedParameter.h"

bool UAnimNextAnimationGraphSchema::SupportsUnitFunction(URigVMController* InController, const FRigVMFunction* InUnitFunction) const
{
	if(InUnitFunction)
	{
		if(const UScriptStruct* FunctionExecuteContextStruct = InUnitFunction->GetExecuteContextStruct())
		{
			if(FunctionExecuteContextStruct == FAnimNextExecuteContext::StaticStruct())
			{
				// Only allow nodes that are children of FRigUnit_AnimNextBase
				if(InUnitFunction->Struct)
				{
					return InUnitFunction->Struct->IsChildOf(FRigUnit_AnimNextBase::StaticStruct());
				}
			}
		}
	}

	return Super::SupportsUnitFunction(InController, InUnitFunction);
}

bool UAnimNextAnimationGraphSchema::SupportsDispatchFactory(URigVMController* InController, const FRigVMDispatchFactory* InDispatchFactory) const
{
	if(InDispatchFactory)
	{
		if(const UScriptStruct* DispatchExecuteContextStruct = InDispatchFactory->GetExecuteContextStruct())
		{
			if(DispatchExecuteContextStruct == FAnimNextExecuteContext::StaticStruct())
			{
				// We only support the FRigVMDispatch_GetParameter/FRigVMDispatch_GetScopedParameter at the moment.
				return
				   (InDispatchFactory->GetScriptStruct() == FRigVMDispatch_GetParameter::StaticStruct() ||
					InDispatchFactory->GetScriptStruct() == FRigVMDispatch_GetScopedParameter::StaticStruct());
			}
		}
	}
	
	return Super::SupportsDispatchFactory(InController, InDispatchFactory);
}