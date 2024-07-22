// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigVMCore/RigVMNodeLayout.h"
#include "RigVMStringUtils.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigVMNodeLayout)

const FString* FRigVMNodeLayout::FindCategory(const FString& InElement) const
{
	for(const FRigVMPinCategory& Category : Categories)
	{
		if(Category.Elements.Contains(InElement))
		{
			return &Category.Path;
		}
	}
	return nullptr;
}

const FString* FRigVMNodeLayout::FindDisplayName(const FString& InElement) const
{
	return DisplayNames.Find(InElement);
}