// Copyright Epic Games, Inc. All Rights Reserved.

#include "Param/AnimNextParameterBlockGraph.h"

FText UAnimNextParameterBlockGraph::GetDisplayName() const
{
	return FText::FromName(GraphName);
}

FText UAnimNextParameterBlockGraph::GetDisplayNameTooltip() const
{
	return FText::FromName(GraphName);
}

void UAnimNextParameterBlockGraph::SetGraphName(FName InName, bool bSetupUndoRedo)
{
	if(bSetupUndoRedo)
	{
		Modify();
	}

	GraphName = InName;

	BroadcastModified();
}