// Copyright Epic Games, Inc. All Rights Reserved.

#include "Param/AnimNextParameterBlockBindingReference.h"

#include "UncookedOnlyUtils.h"
#include "Param/AnimNextParameterBlock.h"

#define LOCTEXT_NAMESPACE "AnimNextParameterBlockBindingReference"

FAnimNextParamType UAnimNextParameterBlockBindingReference::GetParamType() const
{
	if (Block != nullptr)
	{
		if(UAnimNextParameterBlockEntry* ReferencedEntry = UE::AnimNext::UncookedOnly::FUtils::GetEditorData(Block)->FindBinding(ParameterName))
		{
			if(const IAnimNextParameterBlockParameterInterface* Binding = Cast<IAnimNextParameterBlockParameterInterface>(ReferencedEntry))
			{
				return Binding->GetParamType();
			}
		}
	}

	return FAnimNextParamType();
}

FName UAnimNextParameterBlockBindingReference::GetParameterName() const
{
	return ParameterName;
}

void UAnimNextParameterBlockBindingReference::SetParameterName(FName InName, bool bSetupUndoRedo)
{
	ParameterName = InName;
}

const UAnimNextParameterBlock* UAnimNextParameterBlockBindingReference::GetBlock() const
{
	return Block;
}

FText UAnimNextParameterBlockBindingReference::GetDisplayName() const
{
	return FText::FromName(ParameterName);
}

FText UAnimNextParameterBlockBindingReference::GetDisplayNameTooltip() const
{
	FTextBuilder TextBuilder;
	TextBuilder.AppendLine(FText::FromName(ParameterName));
	return TextBuilder.ToText();
}

void UAnimNextParameterBlockBindingReference::GetEditedObjects(TArray<UObject*>& OutObjects) const
{
	if(Block)
	{
		OutObjects.Add(Block);
	}
}

#undef LOCTEXT_NAMESPACE