// Copyright Epic Games, Inc. All Rights Reserved.

#include "Units/RigUnitContext.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigUnitContext)

FName FControlRigExecuteContext::AddRigModuleNameSpace(const FName& InName) const
{
	if(ModuleInstanceName.IsNone())
	{
		return InName;
	}
	return *AddRigModuleNameSpace(InName.ToString());
}

FString FControlRigExecuteContext::AddRigModuleNameSpace(const FString& InName) const
{
	if(ModuleInstanceName.IsNone())
	{
		return InName;
	}
	check(!ModuleInstanceNameSpace.IsEmpty());
	return ModuleInstanceNameSpace + InName;
}

FName FControlRigExecuteContext::RemoveRigModuleNameSpace(const FName& InName) const
{
	if(ModuleInstanceName.IsNone())
	{
		return InName;
	}
	return *RemoveRigModuleNameSpace(InName.ToString());
}

FString FControlRigExecuteContext::RemoveRigModuleNameSpace(const FString& InName) const
{
	if(ModuleInstanceName.IsNone())
	{
		return InName;
	}
	check(!ModuleInstanceNameSpace.IsEmpty());

	if(InName.StartsWith(ModuleInstanceNameSpace, ESearchCase::CaseSensitive))
	{
		return InName.Mid(ModuleInstanceNameSpace.Len());
	}
	return InName;
}
