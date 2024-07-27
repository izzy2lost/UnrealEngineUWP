// Copyright Epic Games, Inc. All Rights Reserved.
#include "ChooserParameters.h"
#include "Param/ParamStack.h"

#if WITH_EDITOR
#include "StructUtils/PropertyBag.h"
#endif

bool FBoolAnimProperty::GetValue(FChooserEvaluationContext& Context, bool& OutResult) const
{
	UE::AnimNext::FParamResult Result;
	UE::AnimNext::FParamStack& ParamStack = UE::AnimNext::FParamStack::Get();
	OutResult = ParamStack.GetParamValue<bool>(UE::AnimNext::FParamId(PropertyName), &Result);
	return Result.IsSuccessful();
}

bool FBoolAnimProperty::SetValue(FChooserEvaluationContext& Context, bool InValue) const
{
	return false;
}

void FBoolAnimProperty::GetDisplayName(FText& OutName) const
{
	FString DisplayName = PropertyName.ToString();
	int Index = -1;
	DisplayName.FindLastChar(':', Index);
	if (Index>=0)
	{
		DisplayName = DisplayName.RightChop(Index + 1);
	}
	
	OutName = FText::FromString(DisplayName);
}


bool FFloatAnimProperty::GetValue(FChooserEvaluationContext& Context, double& OutResult) const
{
	UE::AnimNext::FParamResult Result;
	UE::AnimNext::FParamStack& ParamStack = UE::AnimNext::FParamStack::Get();
	OutResult = ParamStack.GetParamValue<double>(UE::AnimNext::FParamId(PropertyName), &Result);
	return Result.IsSuccessful();
}

bool FFloatAnimProperty::SetValue(FChooserEvaluationContext& Context, double InValue) const
{
	return false;
}

void FFloatAnimProperty::GetDisplayName(FText& OutName) const
{
	FString DisplayName = PropertyName.ToString();
	int Index = -1;
	DisplayName.FindLastChar(':', Index);
	if (Index>=0)
	{
		DisplayName = DisplayName.RightChop(Index + 1);
	}
	
	OutName = FText::FromString(DisplayName);
}