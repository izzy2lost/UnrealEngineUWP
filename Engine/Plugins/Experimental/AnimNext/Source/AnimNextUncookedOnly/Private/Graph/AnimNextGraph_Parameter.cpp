// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/AnimNextGraph_Parameter.h"

#include "UncookedOnlyUtils.h"
#include "Graph/AnimNextGraph.h"
#include "Param/AnimNextTag.h"

FAnimNextParamType UAnimNextGraph_Parameter::GetExportType() const
{
	return GetParamType();
}

FName UAnimNextGraph_Parameter::GetExportName() const
{
	return GetParamName();
}

FAnimNextParamType UAnimNextGraph_Parameter::GetParamType() const
{
	return Type;
}

FName UAnimNextGraph_Parameter::GetEntryName() const
{
	return ParameterName;
}

bool UAnimNextGraph_Parameter::SetParamType(const FAnimNextParamType& InType, bool bSetupUndoRedo)
{
	if(bSetupUndoRedo)
	{
		Modify();
	}
	
	Type = InType;

	BroadcastModified();

	return true;
}

FName UAnimNextGraph_Parameter::GetParamName() const
{
	if(UAnimNextRigVMAsset* OuterAsset = GetTypedOuter<UAnimNextRigVMAsset>())
	{
		return UE::AnimNext::UncookedOnly::FUtils::GetQualifiedName(OuterAsset, ParameterName);
	}
	return ParameterName;
}

void UAnimNextGraph_Parameter::SetParamName(FName InName, bool bSetupUndoRedo)
{
	SetEntryName(InName, bSetupUndoRedo);
}

FInstancedPropertyBag& UAnimNextGraph_Parameter::GetPropertyBag() const
{
	// TODO: move property bag for defaults onto this entry!
	UAnimNextGraph* Asset = GetTypedOuter<UAnimNextGraph>();
	check(Asset);
	return Asset->PropertyBag;
}

void UAnimNextGraph_Parameter::SetEntryName(FName InName, bool bSetupUndoRedo)
{
	if(bSetupUndoRedo)
	{
		Modify();
	}

	ParameterName = InName;
	BroadcastModified();
}

FText UAnimNextGraph_Parameter::GetDisplayName() const
{
	return FText::FromName(ParameterName);
}

FText UAnimNextGraph_Parameter::GetDisplayNameTooltip() const
{
	return FText::FromString(Comment);
}