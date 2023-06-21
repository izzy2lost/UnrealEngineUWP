// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimGraphNode_BlendStack.h"
#include "Animation/AnimRootMotionProvider.h"
#include "AnimationBlendStackGraphSchema.h"
#include "AnimationBlendStackGraph.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "EdGraphUtilities.h"
#include "IAnimBlueprintCompilationContext.h"
#include "AnimGraphNode_Root.h"
#include "AnimGraphNode_BlendStackInput.h"

#define LOCTEXT_NAMESPACE "AnimGraphNode_BlendStack"


FLinearColor UAnimGraphNode_BlendStack::GetNodeTitleColor() const
{
	return FColor(86, 182, 194);
}

FText UAnimGraphNode_BlendStack::GetTooltipText() const
{
	return LOCTEXT("NodeToolTip", "Blend Stack");
}

FText UAnimGraphNode_BlendStack::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("NodeTitle", "Blend Stack");
}

FText UAnimGraphNode_BlendStack::GetMenuCategory() const
{
	return LOCTEXT("NodeCategory", "Pose Search");
}

void UAnimGraphNode_BlendStack_Base::GetOutputLinkAttributes(FNodeAttributeArray& OutAttributes) const
{
	if (UE::Anim::IAnimRootMotionProvider::Get())
	{
		OutAttributes.Add(UE::Anim::IAnimRootMotionProvider::AttributeName);
	}
}

void UAnimGraphNode_BlendStack::BakeDataDuringCompilation(class FCompilerResultsLog& MessageLog)
{
	UAnimBlueprint* AnimBlueprint = GetAnimBlueprint();
	AnimBlueprint->FindOrAddGroup(Node.GetGroupName());
}

void UAnimGraphNode_BlendStack_Base::ExpandGraphAndProcessNodes(
	UEdGraph* SourceGraph, 
	UAnimGraphNode_Base* SourceRootNode, UAnimGraphNode_Base* SourceInputNode,
	IAnimBlueprintCompilationContext& InCompilationContext, IAnimBlueprintGeneratedClassCompiledData& OutCompiledData,
	UAnimGraphNode_Base*& OutRootNode, UAnimGraphNode_Base*& OutInputNode)
{
	// Note: This is mostly copied from UAnimGraphNode_BlendSpaceGraphBase::ExpandGraphAndProcessNodes
	
	// Clone the nodes from the source graph
	// Note that we outer this graph to the ConsolidatedEventGraph to allow ExpansionStep to 
	// correctly retrieve the context for any expanded function calls (custom make/break structs etc.)
	UEdGraph* ClonedGraph = FEdGraphUtilities::CloneGraph(SourceGraph, InCompilationContext.GetConsolidatedEventGraph(), &InCompilationContext.GetMessageLog(), true);

	// Grab all the animation nodes and find the corresponding 
	// root node and input pose node in the cloned set
	TArray<UAnimGraphNode_Base*> AnimNodeList;

	const UObject* SourceRootObject = InCompilationContext.GetMessageLog().FindSourceObject(SourceRootNode);
	const UObject* SourceInputObject = InCompilationContext.GetMessageLog().FindSourceObject(SourceInputNode);
	for (auto NodeIt = ClonedGraph->Nodes.CreateIterator(); NodeIt; ++NodeIt)
	{
		UEdGraphNode* ClonedNode = *NodeIt;

		if (UAnimGraphNode_Base* TestNode = Cast<UAnimGraphNode_Base>(ClonedNode))
		{
			AnimNodeList.Add(TestNode);

			//@TODO: There ought to be a better way to determine this
			const UObject* TestObject = InCompilationContext.GetMessageLog().FindSourceObject(TestNode);
			if (TestObject == SourceRootObject)
			{
				OutRootNode = TestNode;
			}
			if (TestObject == SourceInputObject)
			{
				OutInputNode = TestNode;
			}
		}
	}

	check(OutRootNode && OutInputNode);

	// Run another expansion pass to catch the graph we just added (this is slightly wasteful)
	InCompilationContext.ExpansionStep(ClonedGraph, false);

	// Validate graph now we have expanded/pruned
	InCompilationContext.ValidateGraphIsWellFormed(ClonedGraph);

	// Move the cloned nodes into the consolidated event graph
	const bool bIsLoading = InCompilationContext.GetBlueprint()->bIsRegeneratingOnLoad || IsAsyncLoading();
	const bool bIsCompiling = InCompilationContext.GetBlueprint()->bBeingCompiled;
	ClonedGraph->MoveNodesToAnotherGraph(InCompilationContext.GetConsolidatedEventGraph(), bIsLoading, bIsCompiling);

	// Process any animation nodes
	{
		TArray<UAnimGraphNode_Base*> RootSet;
		RootSet.Add(OutRootNode);

		InCompilationContext.PruneIsolatedAnimationNodes(RootSet, AnimNodeList);
		InCompilationContext.ProcessAnimationNodes(AnimNodeList);
	}
}

void UAnimGraphNode_BlendStack_Base::OnProcessDuringCompilation(IAnimBlueprintCompilationContext& InCompilationContext, IAnimBlueprintGeneratedClassCompiledData& OutCompiledData)
{
	Super::OnProcessDuringCompilation(InCompilationContext, OutCompiledData);

	FAnimNode_BlendStack_Standalone* AnimNode = GetBlendStackNode();
	check(AnimNode);

	const int32 MaxBlendsNum = GetMaxActiveBlends();
	// Set MaxActiveBlends of the blend stack at compile time.
	// @todo: Allow reducing the blend stack size (but not decreasing), to enable scalability control under i.e. lower LODs
	AnimNode->MaxActiveBlends = GetMaxActiveBlends();

	UAnimationBlendStackGraph* SampleGraph = CastChecked<UAnimationBlendStackGraph>(BoundGraph);
	if (!SampleGraph->ResultNode->IsNodeRootSet())
	{
		// Input Pose is connected to Output Pose, so the sample graph does nothing.
		// No need to use allocate sample graphs in that case.
		AnimNode->SampleGraphPoseLinks.Reset();
		return;
	}

	// Allocate one sample graph per-active blend plus an extra one for the stored pose.
	AnimNode->SampleGraphPoseLinks.SetNum(MaxBlendsNum + 1);

	for(FBlendStack_SampleGraphPoseLink& GraphPoseLink : AnimNode->SampleGraphPoseLinks)
	{
		UAnimGraphNode_Base *RootNode, *InputNode;
		ExpandGraphAndProcessNodes(SampleGraph, SampleGraph->ResultNode, SampleGraph->InputNode, InCompilationContext, OutCompiledData, RootNode, InputNode);

		// Blend stack node is potentially nested in the struct, so we can't use FPoseLinkMappingRecord. Patch at runtime instead.
		GraphPoseLink.RootNodeIndex = InCompilationContext.GetAllocationIndexOfNode(RootNode);
		GraphPoseLink.InputPoseNodeIndex = InCompilationContext.GetAllocationIndexOfNode(InputNode);
	}
}

bool UAnimGraphNode_BlendStack_Base::DoesSupportTimeForTransitionGetter() const
{
	return true;
}

UAnimationAsset* UAnimGraphNode_BlendStack_Base::GetAnimationAsset() const
{
	return nullptr;
}

const TCHAR* UAnimGraphNode_BlendStack_Base::GetTimePropertyName() const
{
	return TEXT("InternalTimeAccumulator");
}

UScriptStruct* UAnimGraphNode_BlendStack_Base::GetTimePropertyStruct() const
{
	return FAnimNode_BlendStack::StaticStruct();
}

void UAnimGraphNode_BlendStack_Base::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	if (Ar.IsLoading() && (BoundGraph == nullptr))
	{
		// Update older nodes without a graph to create one on load.
		CreateGraph();
	}
}

int32 UAnimGraphNode_BlendStack_Base::GetMaxActiveBlends() const
{
	FAnimNode_BlendStack_Standalone* AnimNode = GetBlendStackNode();
	check(AnimNode);
	return AnimNode->MaxActiveBlends;
}

void UAnimGraphNode_BlendStack_Base::CreateGraph()
{
	// Create a new animation graph
	check(BoundGraph == nullptr);
	BoundGraph = FBlueprintEditorUtils::CreateNewGraph(
		this,
		NAME_None,
		UAnimationBlendStackGraph::StaticClass(),
		UAnimationBlendStackGraphSchema::StaticClass());
	check(BoundGraph);

	// Initialize the anim graph
	const UEdGraphSchema* Schema = BoundGraph->GetSchema();
	Schema->CreateDefaultNodesForGraph(*BoundGraph);

	// Add the new graph as a child of our parent graph
	UEdGraph* ParentGraph = GetGraph();
	if(ParentGraph->SubGraphs.Find(BoundGraph) == INDEX_NONE)
	{
		ParentGraph->Modify();
		ParentGraph->SubGraphs.Add(BoundGraph);
	}
}

void UAnimGraphNode_BlendStack_Base::PostPlacedNewNode()
{
	Super::PostPlacedNewNode();
	CreateGraph();
}

TArray<UEdGraph*> UAnimGraphNode_BlendStack_Base::GetSubGraphs() const
{
	return TArray<UEdGraph*>({BoundGraph});
}

UObject* UAnimGraphNode_BlendStack_Base::GetJumpTargetForDoubleClick() const
{
	// Open the blend stack graph
	return BoundGraph;
}

void UAnimGraphNode_BlendStack_Base::JumpToDefinition() const
{
	if (UObject* HyperlinkTarget = GetJumpTargetForDoubleClick())
	{
		FKismetEditorUtilities::BringKismetToFocusAttentionOnObject(HyperlinkTarget);
	}
}

void UAnimGraphNode_BlendStack_Base::DestroyNode()
{
	UBlueprint* Blueprint = GetBlueprint();
	UEdGraph* GraphToRemove = BoundGraph;
	BoundGraph = nullptr;
	Super::DestroyNode();

	if (GraphToRemove)
	{
		GraphToRemove->Modify();
		FBlueprintEditorUtils::RemoveGraph(Blueprint, GraphToRemove, EGraphRemoveFlags::Recompile);
	}
}

void UAnimGraphNode_BlendStack_Base::PostPasteNode()
{
	Super::PostPasteNode();

	if(BoundGraph)
	{
		// Add the new graph as a child of our parent graph
		UEdGraph* ParentGraph = GetGraph();

		if(ParentGraph->SubGraphs.Find(BoundGraph) == INDEX_NONE)
		{
			ParentGraph->SubGraphs.Add(BoundGraph);
		}

		for (UEdGraphNode* GraphNode : BoundGraph->Nodes)
		{
			GraphNode->CreateNewGuid();
			GraphNode->PostPasteNode();
			GraphNode->ReconstructNode();
		}

		//restore transactional flag that is lost during copy/paste process
		BoundGraph->SetFlags(RF_Transactional);
	}
}

#undef LOCTEXT_NAMESPACE
