// Copyright Epic Games, Inc. All Rights Reserved.

#include "Module/AnimNextModule_EventGraph.h"

#include "AnimNextRigVMAsset.h"
#include "UncookedOnlyUtils.h"
#include "AnimNextEdGraph.h"

FText UAnimNextModule_EventGraph::GetDisplayName() const
{
	return FText::FromName(GraphName);
}

FText UAnimNextModule_EventGraph::GetDisplayNameTooltip() const
{
	return FText::FromName(GraphName);
}

void UAnimNextModule_EventGraph::SetEntryName(FName InName, bool bSetupUndoRedo)
{
	if(bSetupUndoRedo)
	{
		Modify();
	}

	GraphName = InName;

	BroadcastModified();
}

const FName& UAnimNextModule_EventGraph::GetGraphName() const
{
	return GraphName;
}

URigVMGraph* UAnimNextModule_EventGraph::GetRigVMGraph() const
{
	return Graph;
}

URigVMEdGraph* UAnimNextModule_EventGraph::GetEdGraph() const
{
	return EdGraph;
}

void UAnimNextModule_EventGraph::SetRigVMGraph(URigVMGraph* InGraph)
{
	Graph = InGraph;
}

void UAnimNextModule_EventGraph::SetEdGraph(URigVMEdGraph* InGraph)
{
	EdGraph = CastChecked<UAnimNextEdGraph>(InGraph);
}