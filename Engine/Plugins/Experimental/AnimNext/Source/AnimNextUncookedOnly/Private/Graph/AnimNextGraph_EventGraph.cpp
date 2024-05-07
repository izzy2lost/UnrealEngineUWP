// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/AnimNextGraph_EventGraph.h"

#include "AnimNextRigVMAsset.h"
#include "UncookedOnlyUtils.h"
#include "Graph/AnimNextGraph_EdGraph.h"
#include "Param/AnimNextTag.h"
#include "Param/ParamType.h"

FText UAnimNextGraph_EventGraph::GetDisplayName() const
{
	return FText::FromName(GraphName);
}

FText UAnimNextGraph_EventGraph::GetDisplayNameTooltip() const
{
	return FText::FromName(GraphName);
}

void UAnimNextGraph_EventGraph::SetEntryName(FName InName, bool bSetupUndoRedo)
{
	if(bSetupUndoRedo)
	{
		Modify();
	}

	GraphName = InName;

	BroadcastModified();
}

URigVMGraph* UAnimNextGraph_EventGraph::GetRigVMGraph() const
{
	return Graph;
}

URigVMEdGraph* UAnimNextGraph_EventGraph::GetEdGraph() const
{
	return EdGraph;
}

void UAnimNextGraph_EventGraph::SetRigVMGraph(URigVMGraph* InGraph)
{
	Graph = InGraph;
}

void UAnimNextGraph_EventGraph::SetEdGraph(URigVMEdGraph* InGraph)
{
	EdGraph = CastChecked<UAnimNextGraph_EdGraph>(InGraph);
}