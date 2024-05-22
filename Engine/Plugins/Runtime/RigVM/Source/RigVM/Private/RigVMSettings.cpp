// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigVMSettings.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigVMSettings)

#define LOCTEXT_NAMESPACE "RigVMSettings"

URigVMEditorSettings::URigVMEditorSettings(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
#if WITH_EDITORONLY_DATA
	bAutoLinkMutableNodes = false;
#endif
}

URigVMProjectSettings::URigVMProjectSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	VariantTags.Emplace(TEXT("Outdated"), TEXT("Outdated"), LOCTEXT("OutdatedToolTip", "This item is outdated and should no longer be used."), FLinearColor::Red, true, true);
	VariantTags.Emplace(TEXT("Stable"), TEXT("Stable"), LOCTEXT("StableToolTip", "This item is stable and ready to use."), FLinearColor::Green, true);
}

#undef LOCTEXT_NAMESPACE // RigVMSettings
FRigVMTag URigVMProjectSettings::GetTag(FName InTagName) const
{
	if(const FRigVMTag* FoundTag = FindTag(InTagName))
	{
		return *FoundTag;
	}
	return FRigVMTag();
}

const FRigVMTag* URigVMProjectSettings::FindTag(FName InTagName) const
{
	return VariantTags.FindByPredicate([InTagName](const FRigVMTag& Tag)
	{
		return InTagName == Tag.Name;
	});
}
