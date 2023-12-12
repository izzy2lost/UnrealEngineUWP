// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/AnimNextGraphEntry.h"
#include "RigVMModel/RigVMGraph.h"
#include "Graph/AnimNextGraph_EdGraph.h"

FName UAnimNextGraphEntry::GetEntryName() const
{
	return GraphName;
}

URigVMGraph* UAnimNextGraphEntry::GetRigVMGraph() const
{
	return Graph;
}

URigVMEdGraph* UAnimNextGraphEntry::GetEdGraph() const
{
	return EdGraph;
}
