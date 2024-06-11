// Copyright Epic Games, Inc. All Rights Reserved.

#include "Module/RigUnit_AnimNextModuleSelf.h"
#include "Graph/AnimNextGraphInstance.h"

FRigUnit_AnimNextModuleSelf_Execute()
{
	Self = const_cast<UAnimNextModule*>(ExecuteContext.GetContextData<FAnimNextGraphContextData>().GetGraphInstance().GetModule());
}
