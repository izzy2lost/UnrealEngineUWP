// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/AnimNextGraph_AnimationGraph.h"
#include "Graph/AnimNextGraph_EdGraph.h"
#include "Graph/RigUnit_AnimNextGraphRoot.h"

FName UAnimNextGraph_AnimationGraph::GetEntryName() const
{
	return GraphName;
}

void UAnimNextGraph_AnimationGraph::SetEntryName(FName InName, bool bSetupUndoRedo)
{
	if(bSetupUndoRedo)
	{
		Modify();
	};

	GraphName = InName;

	// Forward to entry point node
	URigVMController* Controller = GetImplementingOuter<IRigVMClientHost>()->GetController(Graph);
	for(URigVMNode* Node : Graph->GetNodes())
	{
		if(URigVMUnitNode* UnitNode = Cast<URigVMUnitNode>(Node))
		{
			if(UnitNode->GetScriptStruct() == FRigUnit_AnimNextGraphRoot::StaticStruct())
			{
				URigVMPin* EntryPointPin = UnitNode->FindPin(GET_MEMBER_NAME_STRING_CHECKED(FRigUnit_AnimNextGraphRoot, EntryPoint));
				check(EntryPointPin);
				check(EntryPointPin->GetDirection() == ERigVMPinDirection::Hidden);

				Controller->SetPinDefaultValue(EntryPointPin->GetPinPath(), InName.ToString());
			}
		}
	}
	
	BroadcastModified();
}

URigVMGraph* UAnimNextGraph_AnimationGraph::GetRigVMGraph() const
{
	return Graph;
}

URigVMEdGraph* UAnimNextGraph_AnimationGraph::GetEdGraph() const
{
	return EdGraph;
}

void UAnimNextGraph_AnimationGraph::SetRigVMGraph(URigVMGraph* InGraph)
{
	Graph = InGraph;
}

void UAnimNextGraph_AnimationGraph::SetEdGraph(URigVMEdGraph* InGraph)
{
	EdGraph = CastChecked<UAnimNextGraph_EdGraph>(InGraph);
}