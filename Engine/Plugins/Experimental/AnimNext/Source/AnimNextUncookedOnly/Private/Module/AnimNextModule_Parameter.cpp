// Copyright Epic Games, Inc. All Rights Reserved.

#include "Module/AnimNextModule_Parameter.h"

#include "UncookedOnlyUtils.h"
#include "Module/AnimNextModule.h"
#include "Param/AnimNextTag.h"

FAnimNextParamType UAnimNextModule_Parameter::GetExportType() const
{
	return GetParamType();
}

FName UAnimNextModule_Parameter::GetExportName() const
{
	return GetParamName();
}

EAnimNextExportAccessSpecifier UAnimNextModule_Parameter::GetExportAccessSpecifier() const
{
	return Access;
}

void UAnimNextModule_Parameter::SetExportAccessSpecifier(EAnimNextExportAccessSpecifier InAccessSpecifier, bool bSetupUndoRedo)
{
	if(bSetupUndoRedo)
	{
		Modify();
	};

	Access = InAccessSpecifier;

	BroadcastModified();
}

FAnimNextParamType UAnimNextModule_Parameter::GetParamType() const
{
	return Type;
}

FName UAnimNextModule_Parameter::GetEntryName() const
{
	return ParameterName;
}

bool UAnimNextModule_Parameter::SetParamType(const FAnimNextParamType& InType, bool bSetupUndoRedo)
{
	if(bSetupUndoRedo)
	{
		Modify();
	}
	
	Type = InType;

	BroadcastModified();

	return true;
}

FName UAnimNextModule_Parameter::GetParamName() const
{
	if(UAnimNextRigVMAsset* OuterAsset = GetTypedOuter<UAnimNextRigVMAsset>())
	{
		return UE::AnimNext::UncookedOnly::FUtils::GetQualifiedName(OuterAsset, ParameterName);
	}
	return ParameterName;
}

void UAnimNextModule_Parameter::SetParamName(FName InName, bool bSetupUndoRedo)
{
	SetEntryName(InName, bSetupUndoRedo);
}

FInstancedPropertyBag& UAnimNextModule_Parameter::GetPropertyBag() const
{
	// TODO: move property bag for defaults onto this entry!
	UAnimNextModule* Asset = GetTypedOuter<UAnimNextModule>();
	check(Asset);
	return Asset->DefaultState.State;
}

void UAnimNextModule_Parameter::SetEntryName(FName InName, bool bSetupUndoRedo)
{
	if(bSetupUndoRedo)
	{
		Modify();
	}

	ParameterName = InName;
	BroadcastModified();
}

FText UAnimNextModule_Parameter::GetDisplayName() const
{
	return FText::FromName(ParameterName);
}

FText UAnimNextModule_Parameter::GetDisplayNameTooltip() const
{
	return FText::FromString(Comment);
}