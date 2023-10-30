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

FName FControlRigExecuteContext::AdaptMetadataName(bool bUseNameSpace, const FName& InMetadataName) const
{
	// only if we are within a rig module let's adapt the meta data name
	if(bUseNameSpace && IsRigModule() && !InMetadataName.IsNone())
	{
		// if the metadata name already contains a namespace - we are just going
		// to use it as is. this means that modules have access to other module's metadata,
		// and that's ok. the user will require the full path to it anyway so it is a
		// conscious user decision.
		const FString MetadataNameString = InMetadataName.ToString();
		int32 Index = INDEX_NONE;
		if(MetadataNameString.FindChar(TEXT(':'), Index))
		{
			return InMetadataName;
		}

		// prefix the meta data name with the namespace to allow modules to store their
		// metadata in a way that doesn't collide with other modules' metadata.
		const FString JoinedMetadataName = GetRigModuleNameSpace() + MetadataNameString;
		return *JoinedMetadataName;
	}
	return InMetadataName;
}

FControlRigExecuteContextRigModuleGuard::FControlRigExecuteContextRigModuleGuard(FControlRigExecuteContext& InContext, const UControlRig* InControlRig)
	: Context(InContext)
	, PreviousRigModuleNameSpace(InContext.RigModuleNameSpace)
	, PreviousRigModuleNameSpaceHash(InContext.RigModuleNameSpaceHash)
{
	Context.RigModuleNameSpace = InControlRig->GetRigModuleNameSpace();
	Context.RigModuleNameSpaceHash = GetTypeHash(Context.RigModuleNameSpace);
}

FControlRigExecuteContextRigModuleGuard::FControlRigExecuteContextRigModuleGuard(FControlRigExecuteContext& InContext, const FString& InNewModuleNameSpace)
	: Context(InContext)
	, PreviousRigModuleNameSpace(InContext.RigModuleNameSpace)
	, PreviousRigModuleNameSpaceHash(InContext.RigModuleNameSpaceHash)
{
	Context.RigModuleNameSpace = InNewModuleNameSpace;
	Context.RigModuleNameSpaceHash = GetTypeHash(Context.RigModuleNameSpace);
}

FControlRigExecuteContextRigModuleGuard::~FControlRigExecuteContextRigModuleGuard()
{
	Context.RigModuleNameSpace = PreviousRigModuleNameSpace;
	Context.RigModuleNameSpaceHash = PreviousRigModuleNameSpaceHash; 
}
