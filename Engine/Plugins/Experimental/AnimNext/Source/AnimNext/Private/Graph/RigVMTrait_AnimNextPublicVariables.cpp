// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/RigVMTrait_AnimNextPublicVariables.h"
#include "AnimNextRigVMAsset.h"
#if WITH_EDITOR
#include "RigVMModel/RigVMController.h"
#include "RigVMModel/RigVMPin.h"
#endif

#if WITH_EDITOR
FString FRigVMTrait_AnimNextPublicVariables::GetDisplayName() const
{
	TStringBuilder<256> StringBuilder;
	StringBuilder.Appendf(TEXT("Variables: %s"), Asset ? *Asset->GetFName().ToString() : TEXT("None"));
	return StringBuilder.ToString();
}

void FRigVMTrait_AnimNextPublicVariables::GetProgrammaticPins(URigVMController* InController, int32 InParentPinIndex, const FString& InDefaultValue, FRigVMPinInfoArray& OutPinArray) const
{
	if (Asset == nullptr)
	{
		return;
	}

	const FInstancedPropertyBag& UserDefaults = Asset->GetPublicVariableDefaults();
	if(!UserDefaults.IsValid())
	{
		return;
	}

	FInstancedPropertyBag Defaults;
	Defaults.InitializeFromBagStruct(UserDefaults.GetPropertyBagStruct());

	const TFunction<ERigVMPinDefaultValueType(const FName&)> DefaultValueTypeGetter = [&UserDefaults, &Defaults](const FName& InPropertyName)
	{
		if(const FProperty* Property = UserDefaults.GetPropertyBagStruct()->FindPropertyByName(InPropertyName))
		{
			if(Property->Identical_InContainer(UserDefaults.GetValue().GetMemory(), Defaults.GetValue().GetMemory()))
			{
				return ERigVMPinDefaultValueType::Unset;
			}
			return ERigVMPinDefaultValueType::Override;
		}
		return ERigVMPinDefaultValueType::AutoDetect;
	};

	OutPinArray.AddPins(const_cast<UPropertyBag*>(UserDefaults.GetPropertyBagStruct()), InController, ERigVMPinDirection::Input, InParentPinIndex, DefaultValueTypeGetter, UserDefaults.GetValue().GetMemory(), true);
}

bool FRigVMTrait_AnimNextPublicVariables::ShouldCreatePinForProperty(const FProperty* InProperty) const
{
	if(!Super::ShouldCreatePinForProperty(InProperty))
	{
		return false;
	}
	return
		InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(FRigVMTrait_AnimNextPublicVariables, Asset) ||
		InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(FRigVMTrait_AnimNextPublicVariables, VariableNames) ||
		VariableNames.Contains(InProperty->GetFName());
}
#endif