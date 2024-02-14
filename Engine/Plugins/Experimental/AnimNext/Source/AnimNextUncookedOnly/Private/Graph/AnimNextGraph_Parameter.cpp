// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/AnimNextGraph_Parameter.h"
#include "Graph/AnimNextGraph.h"
#include "Param/ExternalParameterRegistry.h"

FAnimNextParamType UAnimNextGraph_Parameter::GetParamType() const
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
	using namespace UE::AnimNext;

	IParameterSourceFactory::FParameterInfo Info;
	if(FExternalParameterRegistry::FindParameterInfo(ParameterName, Info))
	{
		return Info.Tooltip;
	}
	
	return FText::FromName(ParameterName);
}