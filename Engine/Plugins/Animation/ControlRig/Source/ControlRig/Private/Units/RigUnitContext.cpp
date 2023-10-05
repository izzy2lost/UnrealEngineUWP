// Copyright Epic Games, Inc. All Rights Reserved.

#include "Units/RigUnitContext.h"
#include "ControlRig.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigUnitContext)

FName FControlRigExecuteContext::AddRigModuleNameSpace(const FName& InName) const
{
	if(IsRigModule())
	{
		return InName;
	}
	return *AddRigModuleNameSpace(InName.ToString());
}

FString FControlRigExecuteContext::AddRigModuleNameSpace(const FString& InName) const
{
	if(IsRigModule())
	{
		return InName;
	}
	check(!RigModuleNameSpace.IsEmpty());
	return RigModuleNameSpace + InName;
}

FName FControlRigExecuteContext::RemoveRigModuleNameSpace(const FName& InName) const
{
	if(IsRigModule())
	{
		return InName;
	}
	return *RemoveRigModuleNameSpace(InName.ToString());
}

FString FControlRigExecuteContext::RemoveRigModuleNameSpace(const FString& InName) const
{
	if(IsRigModule())
	{
		return InName;
	}
	check(!RigModuleNameSpace.IsEmpty());

	if(InName.StartsWith(RigModuleNameSpace, ESearchCase::CaseSensitive))
	{
		return InName.Mid(RigModuleNameSpace.Len());
	}
	return InName;
}

FControlRigExecuteContextRigModuleGuard::FControlRigExecuteContextRigModuleGuard(FControlRigExecuteContext& InContext, const UControlRig* InControlRig)
	: Context(InContext)
	, PreviousRigModuleNameSpace(InContext.RigModuleNameSpace)
	, PreviousRigModuleNameSpaceHash(InContext.RigModuleNameSpaceHash)
{
	Context.RigModuleNameSpace = InControlRig->GetRigModuleNameSpace();
	Context.RigModuleNameSpaceHash = GetTypeHash(Context.RigModuleNameSpace);
}
	
FControlRigExecuteContextRigModuleGuard::~FControlRigExecuteContextRigModuleGuard()
{
	Context.RigModuleNameSpace = PreviousRigModuleNameSpace;
	Context.RigModuleNameSpaceHash = PreviousRigModuleNameSpaceHash; 
}
