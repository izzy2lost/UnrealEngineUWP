// Copyright Epic Games, Inc. All Rights Reserved.

#include "Playback/Graph/AvaPlaybackGraphSchema.h"
#include "AvaBlueprint.h"
#include "GraphEditorActions.h"
#include "Playback/AvalanchePlayback.h"
#include "Playback/Graph/AvaPlaybackGraph.h"
#include "Playback/Graph/Nodes/AvaPlaybackGraphNode_Root.h"
#include "Playback/Graph/SchemaActions/AvaPlaybackAction_NewComment.h"
#include "Playback/Graph/SchemaActions/AvaPlaybackAction_NewNode.h"
#include "Playback/Graph/SchemaActions/AvaPlaybackAction_PasteNode.h"
#include "Playback/IAvaPlaybackEditor.h"
#include "Playback/Nodes/AvaPlaybackNodeRoot.h"
#include "ScopedTransaction.h"
#include "ToolMenu.h"
#include "UObject/UObjectIterator.h"

#define LOCTEXT_NAMESPACE "AvaPlaybackGraphSchema"

const FLinearColor UAvaPlaybackGraphSchema::ActivePinColor = FLinearColor::White;
const FLinearColor UAvaPlaybackGraphSchema::InactivePinColor = FLinearColor(0.05f, 0.05f, 0.05f);

// Allowable PinType.PinCategory values
const FName UAvaPlaybackGraphSchema::PC_ChannelFeed(TEXT("channelfeed"));
const FName UAvaPlaybackGraphSchema::PC_Event(TEXT("event"));

TArray<TSubclassOf<UAvaPlaybackNode>> UAvaPlaybackGraphSchema::PlaybackNodeClasses;

TSharedPtr<IAvaPlaybackGraphEditor> UAvaPlaybackGraphSchema::GetPlaybackGraphEditor(const UEdGraph* Graph) const
{
	if (Graph && Cast<UAvaPlaybackGraph>(Graph))
	{
		if (UAvalanchePlayback* const Playback = Cast<UAvaPlaybackGraph>(Graph)->GetPlayback())
		{
			return Playback->GetGraphEditor();
		}
	}
	return nullptr;
}

void UAvaPlaybackGraphSchema::CompilePlaybackNodesFromGraphNodes(UEdGraphNode& Node) const
{
	CastChecked<UAvaPlaybackGraph>(Node.GetGraph())->GetPlayback()->CompilePlaybackNodesFromGraphNodes();
}

bool UAvaPlaybackGraphSchema::ConnectionCausesLoop(const UEdGraphPin* InputPin, const UEdGraphPin* OutputPin) const
{
	UAvaPlaybackGraphNode* const InputNode = Cast<UAvaPlaybackGraphNode>(InputPin->GetOwningNode());

	if (InputNode)
	{
		// Only nodes representing SoundNodes have outputs
		UAvaPlaybackGraphNode* const OutputNode = CastChecked<UAvaPlaybackGraphNode>(OutputPin->GetOwningNode());

		if (OutputNode->GetPlaybackNode())
		{
			// Grab all child nodes. We can't just test the output because 
			// the loop could happen from any additional child nodes. 
			TArray<UAvaPlaybackNode*> Nodes;
			OutputNode->GetPlaybackNode()->GetAllNodes(Nodes);

			// If our test input is in that set, return true.
			return Nodes.Contains(InputNode->GetPlaybackNode());
		}
	}

	// Simple connection to root node
	return false;
}

void UAvaPlaybackGraphSchema::GetGraphContextActions(FGraphContextMenuBuilder& ContextMenuBuilder) const
{
	GetPlaybackNodeActions(ContextMenuBuilder, true);
	GetCommentAction(ContextMenuBuilder, ContextMenuBuilder.CurrentGraph);

	bool bCanPasteNodes = false;
	if (TSharedPtr<IAvaPlaybackGraphEditor> GraphEditor = GetPlaybackGraphEditor(ContextMenuBuilder.CurrentGraph))
	{
		bCanPasteNodes = GraphEditor->CanPasteNodes();
	}
	
	if (!ContextMenuBuilder.FromPin && bCanPasteNodes)
	{
		TSharedPtr<FAvaPlaybackAction_PasteNode> NewAction = MakeShared<FAvaPlaybackAction_PasteNode>(FText::GetEmpty()
			, LOCTEXT("PasteHereAction", "Paste here")
			, FText::GetEmpty()
			, 0);
		
		ContextMenuBuilder.AddAction(NewAction);
	}
}

void UAvaPlaybackGraphSchema::GetContextMenuActions(UToolMenu* Menu, UGraphNodeContextMenuContext* Context) const
{
	if (Context->Node)
	{
		const UAvaPlaybackGraphNode* const GraphNode = Cast<const UAvaPlaybackGraphNode>(Context->Node);
		{
			FToolMenuSection& Section = Menu->AddSection("AvaGraphSchemaNodeActions", LOCTEXT("NodeActionsMenuHeader", "Node Actions"));
			Section.AddMenuEntry(FGraphEditorCommands::Get().BreakNodeLinks);
		}
	}
	Super::GetContextMenuActions(Menu, Context);
}

void UAvaPlaybackGraphSchema::CreateDefaultNodesForGraph(UEdGraph& Graph) const
{
	UAvalanchePlayback* const Playback = CastChecked<UAvaPlaybackGraph>(&Graph)->GetPlayback();
	UAvaPlaybackNodeRoot* const RootNode = Playback->ConstructPlaybackNode<UAvaPlaybackNodeRoot>();
	check(RootNode);
	
	SetNodeMetaData(RootNode->GetGraphNode(), FNodeMetadata::DefaultGraphNode);
}

const FPinConnectionResponse UAvaPlaybackGraphSchema::CanCreateConnection(const UEdGraphPin* PinA
	, const UEdGraphPin* PinB) const
{
	// Make sure the pins are not on the same node
	if (PinA->GetOwningNode() == PinB->GetOwningNode())
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, LOCTEXT("ConnectionSameNode", "Both are on the same node"));
	}

	// Pin mismatch in Pin Category
	if (PinA->PinType.PinCategory != PinB->PinType.PinCategory)
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, LOCTEXT("IncompatibleCategories", "Pin Types are not Compatible"));
	}
	
	// Compare the directions
	const UEdGraphPin* InputPin = nullptr;
	const UEdGraphPin* OutputPin = nullptr;

	if (!CategorizePinsByDirection(PinA, PinB, InputPin, OutputPin))
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, LOCTEXT("IncompatibleDirections", "Directions are not compatible"));
	}

	if (ConnectionCausesLoop(InputPin, OutputPin))
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, LOCTEXT("ConnectionLoop", "Connection would cause loop"));
	}

	// Break existing connections on inputs only for Channel Feed only
	// multiple Output connections are acceptable
	if (InputPin->LinkedTo.Num() > 0)
	{
		ECanCreateConnectionResponse ReplyBreakOutputs;
		if (InputPin == PinA)
		{
			ReplyBreakOutputs = CONNECT_RESPONSE_BREAK_OTHERS_A;
		}
		else
		{
			ReplyBreakOutputs = CONNECT_RESPONSE_BREAK_OTHERS_B;
		}
		return FPinConnectionResponse(ReplyBreakOutputs, LOCTEXT("ConnectionReplace", "Replace existing connections"));
	}

	return FPinConnectionResponse(CONNECT_RESPONSE_MAKE, TEXT(""));
}

bool UAvaPlaybackGraphSchema::TryCreateConnection(UEdGraphPin* PinA, UEdGraphPin* PinB) const
{
	if (UEdGraphSchema::TryCreateConnection(PinA, PinB))
	{
		CompilePlaybackNodesFromGraphNodes(*PinA->GetOwningNode());
		return true;
	}
	return false;
}

FLinearColor UAvaPlaybackGraphSchema::GetPinTypeColor(const FEdGraphPinType& PinType) const
{
	if (PinType.PinCategory == PC_ChannelFeed)
	{
		return FLinearColor::White;
	}
	
	if (PinType.PinCategory == PC_Event)
	{
		return FLinearColor::White;
	}

	return Super::GetPinTypeColor(PinType);
}

bool UAvaPlaybackGraphSchema::ShouldHidePinDefaultValue(UEdGraphPin* Pin) const
{
	return Super::ShouldHidePinDefaultValue(Pin);
}

void UAvaPlaybackGraphSchema::BreakNodeLinks(UEdGraphNode& TargetNode) const
{
	Super::BreakNodeLinks(TargetNode);
	CompilePlaybackNodesFromGraphNodes(TargetNode);
}

void UAvaPlaybackGraphSchema::BreakPinLinks(UEdGraphPin& TargetPin, bool bSendsNodeNotifcation) const
{
	const FScopedTransaction Transaction(LOCTEXT("PlaybackGraph_BreakPinLinks", "Break Pin Links"));
	Super::BreakPinLinks(TargetPin, bSendsNodeNotifcation);

	// if this would notify the node then we need to compile the SoundCue
	if (bSendsNodeNotifcation)
	{
		CompilePlaybackNodesFromGraphNodes(*TargetPin.GetOwningNode());
	}
}

void UAvaPlaybackGraphSchema::GetAssetsGraphHoverMessage(const TArray<FAssetData>& Assets, const UEdGraph* HoverGraph
	, FString& OutTooltipText, bool& OutOkIcon) const
{
	Super::GetAssetsGraphHoverMessage(Assets, HoverGraph, OutTooltipText, OutOkIcon);
}

void UAvaPlaybackGraphSchema::DroppedAssetsOnGraph(const TArray<FAssetData>& Assets, const FVector2D& GraphPosition
	, UEdGraph* Graph) const
{
	Super::DroppedAssetsOnGraph(Assets, GraphPosition, Graph);
}

void UAvaPlaybackGraphSchema::DroppedAssetsOnNode(const TArray<FAssetData>& Assets, const FVector2D& GraphPosition
	, UEdGraphNode* Node) const
{
	Super::DroppedAssetsOnNode(Assets, GraphPosition, Node);
}

int32 UAvaPlaybackGraphSchema::GetNodeSelectionCount(const UEdGraph* Graph) const
{
	return Super::GetNodeSelectionCount(Graph);
}

TSharedPtr<FEdGraphSchemaAction> UAvaPlaybackGraphSchema::GetCreateCommentAction() const
{
	return MakeShared<FAvaPlaybackAction_NewComment>();
}

void UAvaPlaybackGraphSchema::CachePlaybackNodeClasses()
{
	if (PlaybackNodeClasses.IsEmpty())
	{
		// Construct list of non-abstract sound node classes.
		for (UClass* Class : TObjectRange<UClass>())
		{
			if (Class->IsChildOf(UAvaPlaybackNode::StaticClass()) && !Class->HasAnyClassFlags(CLASS_Abstract))
			{
				PlaybackNodeClasses.Add(Class);
			}
		}
	}
}

void UAvaPlaybackGraphSchema::GetPlaybackNodeActions(FGraphActionMenuBuilder& ActionMenuBuilder, bool bShowSelectedActions) const
{
	CachePlaybackNodeClasses();
	
	for (TSubclassOf<UAvaPlaybackNode> PlaybackNodeClass : PlaybackNodeClasses)
	{		
		UAvaPlaybackNode* PlaybackNode = PlaybackNodeClass->GetDefaultObject<UAvaPlaybackNode>();
		
		if (!ActionMenuBuilder.FromPin
			|| ActionMenuBuilder.FromPin->Direction == EGPD_Input
			|| PlaybackNode->GetMaxChildNodes() > 0)
		{
			const FText NodeTitle = PlaybackNode->GetNodeDisplayNameText();
			const FText NodeCategory = PlaybackNode->GetNodeCategoryText();
			
			//New Node Action (for anything other than Playback Node Root)
			if (!PlaybackNodeClass->IsChildOf(UAvaPlaybackNodeRoot::StaticClass()))
			{				
				FFormatNamedArguments Arguments;
				Arguments.Add(TEXT("Desc"), NodeTitle);
				
				TSharedPtr<FAvaPlaybackAction_NewNode> NewNodeAction = MakeShared<FAvaPlaybackAction_NewNode>(NodeCategory
					, NodeTitle
					, FText::Format(LOCTEXT("NewPlaybackNodeTooltip", "Adds {Desc} node here"), Arguments)
					, 0);
				
				ActionMenuBuilder.AddAction(NewNodeAction);
				NewNodeAction->SetPlaybackNodeClass(PlaybackNodeClass);
			}
		}
	}
}

void UAvaPlaybackGraphSchema::GetCommentAction(FGraphActionMenuBuilder& ActionMenuBuilder, const UEdGraph* CurrentGraph) const
{
	if (!ActionMenuBuilder.FromPin)
	{
		TSharedPtr<FAvaPlaybackAction_NewComment> NewAction = MakeShared<FAvaPlaybackAction_NewComment>(FText::GetEmpty()
			, LOCTEXT("AddCommentAction", "Add Comment...")
			, LOCTEXT("CreateCommentToolTip", "Creates a comment.")
			, 0);
		
		ActionMenuBuilder.AddAction(NewAction);
	}
}

#undef LOCTEXT_NAMESPACE 
