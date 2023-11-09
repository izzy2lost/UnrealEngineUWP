// Copyright Epic Games, Inc. All Rights Reserved.

#include "Param/Params.h"

#define LOCTEXT_NAMESPACE "AnimNextBuiltInParams"

namespace UE::AnimNext
{

static TMap<FName, FParamDefinition> RegisteredParams;

FParamDefinition FParams::RegisterBuiltInParameter(const FParamDefinition& InDefinition)
{
	return RegisteredParams.Add(InDefinition.GetName(), InDefinition);
}

void FParams::UnregisterBuiltInParameter(FName InName)
{
	RegisteredParams.Remove(InName);
}

bool FParams::IsBuiltInParameter(FName InName)
{
	return RegisteredParams.Contains(InName);
}

const FParamDefinition& FParams::GetBuiltInParameter(FName InName)
{
	return RegisteredParams.FindChecked(InName);
}

const FParamDefinition* FParams::FindBuiltInParameter(FName InName)
{
	return RegisteredParams.Find(InName);
}

int32 FParams::GetNumBuiltInParameters()
{
	return RegisteredParams.Num();
}

void FParams::ForEachBuiltInParameter(TFunctionRef<void(const FParamDefinition&)> InFunction)
{
	for (const TPair<FName, FParamDefinition>& RegisteredParamPair : RegisteredParams)
	{
		InFunction(RegisteredParamPair.Value);
	}
}

}

#undef LOCTEXT_NAMESPACE