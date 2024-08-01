// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/AnimNextModule_AnimationGraph.h"

#include "AnimNextRigVMAsset.h"
#include "UncookedOnlyUtils.h"
#include "AnimNextEdGraph.h"
#include "Graph/RigUnit_AnimNextGraphRoot.h"
#include "Param/AnimNextTag.h"
#include "Param/ParamType.h"

FAnimNextParamType UAnimNextModule_AnimationGraph::GetExportType() const
{
	return FAnimNextParamType::GetType<FAnimNextEntryPoint>();
}

FName UAnimNextModule_AnimationGraph::GetExportName() const
{
	if(UAnimNextRigVMAsset* OuterAsset = GetTypedOuter<UAnimNextRigVMAsset>())
	{
		return UE::AnimNext::UncookedOnly::FUtils::GetQualifiedName(OuterAsset, GraphName);
	}
	return GraphName;
}

EAnimNextExportAccessSpecifier UAnimNextModule_AnimationGraph::GetExportAccessSpecifier() const
{
	return Access;
}

void UAnimNextModule_AnimationGraph::SetExportAccessSpecifier(EAnimNextExportAccessSpecifier InAccessSpecifier, bool bSetupUndoRedo)
{
	if(bSetupUndoRedo)
	{
		Modify();
	};

	Access = InAccessSpecifier;

	BroadcastModified();
}

FName UAnimNextModule_AnimationGraph::GetEntryName() const
{
	return GraphName;
}

void UAnimNextModule_AnimationGraph::SetEntryName(FName InName, bool bSetupUndoRedo)
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

const FName& UAnimNextModule_AnimationGraph::GetGraphName() const
{
	return GraphName;
}

URigVMGraph* UAnimNextModule_AnimationGraph::GetRigVMGraph() const
{
	return Graph;
}

URigVMEdGraph* UAnimNextModule_AnimationGraph::GetEdGraph() const
{
	return EdGraph;
}

void UAnimNextModule_AnimationGraph::SetRigVMGraph(URigVMGraph* InGraph)
{
	Graph = InGraph;
}

void UAnimNextModule_AnimationGraph::SetEdGraph(URigVMEdGraph* InGraph)
{
	EdGraph = CastChecked<UAnimNextEdGraph>(InGraph);
}
