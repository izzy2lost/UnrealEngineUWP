// Copyright Epic Games, Inc. All Rights Reserved.

#include "Param/AnimNextParameterBlockParameter.h"
#include "Param/AnimNextParameterBlock_EditorData.h"
#include "Param/ExternalParameterRegistry.h"

FAnimNextParamType UAnimNextParameterBlockParameter::GetParamType() const
{
	using namespace UE::AnimNext;

	// Look in built-in parameters
	IParameterSourceFactory::FParameterInfo Info;
	if(FExternalParameterRegistry::FindParameterInfo(ParameterName, Info))
	{
		return Info.Type;
	}

	return Type;
}

FName UAnimNextParameterBlockParameter::GetParameterName() const
{
	return ParameterName;
}

bool UAnimNextParameterBlockParameter::SetParamType(const FAnimNextParamType& InType, bool bSetupUndoRedo)
{
	if(bSetupUndoRedo)
	{
		Modify();
	}
	
	Type = InType;

	BroadcastModified();

	return true;
}

void UAnimNextParameterBlockParameter::SetParameterName(FName InName, bool bSetupUndoRedo)
{
	if(bSetupUndoRedo)
	{
		Modify();
	}

	ParameterName = InName;
	BroadcastModified();
}

FText UAnimNextParameterBlockParameter::GetDisplayName() const
{
	return FText::FromName(ParameterName);
}

FText UAnimNextParameterBlockParameter::GetDisplayNameTooltip() const
{
	using namespace UE::AnimNext;

	IParameterSourceFactory::FParameterInfo Info;
	if(FExternalParameterRegistry::FindParameterInfo(ParameterName, Info))
	{
		return Info.Tooltip;
	}
	
	return FText::FromName(ParameterName);
}