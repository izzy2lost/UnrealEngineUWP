// Copyright Epic Games, Inc. All Rights Reserved.

#include "Param/AnimNextParameterBlockParameter.h"
#include "Param/AnimNextParameter.h"
#include "Param/AnimNextParameterLibrary.h"
#include "Param/Params.h"

FAnimNextParamType UAnimNextParameterBlockParameter::GetParamType() const
{
	using namespace UE::AnimNext;
	
	if(Library)
	{
		if(const UAnimNextParameter* Parameter = Library->FindParameter(ParameterName))
		{
			return Parameter->GetType();
		}
	}
	else
	{
		// Look in built-in parameters
		if(const FParamDefinition* ParamDefinition = FParams::FindBuiltInParameter(ParameterName))
		{
			return ParamDefinition->GetType();
		}
	}

	return FAnimNextParamType();
}

FName UAnimNextParameterBlockParameter::GetParameterName() const
{
	return ParameterName;
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

const UAnimNextParameter* UAnimNextParameterBlockParameter::GetParameter() const
{
	if(Library)
	{
		return Library->FindParameter(ParameterName);
	}

	return nullptr;
}

const UAnimNextParameterLibrary* UAnimNextParameterBlockParameter::GetLibrary() const
{
	return Library;
}

FText UAnimNextParameterBlockParameter::GetDisplayName() const
{
	return FText::FromName(ParameterName);
}

FText UAnimNextParameterBlockParameter::GetDisplayNameTooltip() const
{
	return FText::FromName(ParameterName);
}