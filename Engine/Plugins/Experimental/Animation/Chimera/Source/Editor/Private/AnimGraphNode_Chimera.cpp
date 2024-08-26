// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimGraphNode_Chimera.h"

#define LOCTEXT_NAMESPACE "Chimera"

UAnimGraphNode_Chimera::UAnimGraphNode_Chimera(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}

FText UAnimGraphNode_Chimera::GetTooltipText() const
{
	return LOCTEXT("ChimeraTooltip", "Character Interaction Matched Node");
}

FText UAnimGraphNode_Chimera::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("Chimera", "Chimera");
}

FLinearColor UAnimGraphNode_Chimera::GetNodeTitleColor() const
{
	return FLinearColor(FColor(6, 9, 53));
}

#undef LOCTEXT_NAMESPACE
