// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/AnimNextGraph_AnimationGraph.h"

#include "AnimNextRigVMAsset.h"
#include "UncookedOnlyUtils.h"
#include "Graph/AnimNextGraph_EdGraph.h"
#include "Graph/RigUnit_AnimNextGraphRoot.h"
#include "Param/AnimNextTag.h"
#include "Param/ParamType.h"

FAnimNextParamType UAnimNextGraph_AnimationGraph::GetExportType() const
{
	return FAnimNextParamType::GetType<FAnimNextEntryPoint>();
}

FName UAnimNextGraph_AnimationGraph::GetExportName() const
{
	if(UAnimNextRigVMAsset* OuterAsset = GetTypedOuter<UAnimNextRigVMAsset>())
	{
		return UE::AnimNext::UncookedOnly::FUtils::GetQualifiedName(OuterAsset, GraphName);
	}
	return GraphName;
}

EAnimNextExportAccessSpecifier UAnimNextGraph_AnimationGraph::GetExportAccessSpecifier() const
{
	return Access;
}

void UAnimNextGraph_AnimationGraph::SetExportAccessSpecifier(EAnimNextExportAccessSpecifier InAccessSpecifier, bool bSetupUndoRedo)
{
	if(bSetupUndoRedo)
	{
		Modify();
	};

	Access = InAccessSpecifier;

	BroadcastModified();
}

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
