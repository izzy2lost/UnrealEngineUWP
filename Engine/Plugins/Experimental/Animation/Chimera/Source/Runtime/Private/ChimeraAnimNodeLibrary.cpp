// Copyright Epic Games, Inc. All Rights Reserved.

#include "Chimera/ChimeraAnimNodeLibrary.h"
#include "Chimera/AnimNode_Chimera.h"
#include "Chimera/ChimeraDefines.h"

FChimeraAnimNodeReference UChimeraAnimNodeLibrary::ConvertToChimeraNode(const FAnimNodeReference& Node, EAnimNodeReferenceConversionResult& Result)
{
	return FAnimNodeReference::ConvertToType<FChimeraAnimNodeReference>(Node, Result);
}

void UChimeraAnimNodeLibrary::SetAvailabilities(const FChimeraAnimNodeReference& ChimeraNode, const TArray<FChimeraAvailability>& Availabilities)
{
	if (FAnimNode_Chimera* ChimeraNodePtr = ChimeraNode.GetAnimNodePtr<FAnimNode_Chimera>())
	{
		ChimeraNodePtr->Availabilities = Availabilities;
	}
	else
	{
		UE_LOG(LogChimera, Warning, TEXT("UChimeraAnimNodeLibrary::SetAvailabilities called on an invalid context or with an invalid type"));
	}
}