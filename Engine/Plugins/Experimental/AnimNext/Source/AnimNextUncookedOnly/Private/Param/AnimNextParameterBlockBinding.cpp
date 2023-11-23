// Copyright Epic Games, Inc. All Rights Reserved.

#include "Param/AnimNextParameterBlockBinding.h"

#define LOCTEXT_NAMESPACE "AnimNextParameterBlockBinding"

FAnimNextParamType UAnimNextParameterBlockBinding::GetParamType() const
{
	return Type;
}

FName UAnimNextParameterBlockBinding::GetParameterName() const
{
	return ParameterName;
}

bool UAnimNextParameterBlockBinding::SetParamType(const FAnimNextParamType& InType, bool bSetupUndoRedo)
{
	if(bSetupUndoRedo)
	{
		Modify();
	}
	
	Type = InType;

	BroadcastModified();

	return true;
}

void UAnimNextParameterBlockBinding::SetParameterName(FName InName, bool bSetupUndoRedo)
{
	if(bSetupUndoRedo)
	{
		Modify();
	}

	ParameterName = InName;

	BroadcastModified();
}

FText UAnimNextParameterBlockBinding::GetDisplayName() const
{
	return FText::FromName(ParameterName);
}

FText UAnimNextParameterBlockBinding::GetDisplayNameTooltip() const
{
	FTextBuilder TextBuilder;
	TextBuilder.AppendLine(FText::FromName(ParameterName));
	return TextBuilder.ToText();
}

#undef LOCTEXT_NAMESPACE