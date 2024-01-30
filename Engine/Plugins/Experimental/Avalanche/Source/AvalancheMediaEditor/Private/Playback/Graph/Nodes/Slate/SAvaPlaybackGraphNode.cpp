// Copyright Epic Games, Inc. All Rights Reserved.

#include "Playback/Graph/Nodes/Slate/SAvaPlaybackGraphNode.h"
#include "GraphEditorSettings.h"
#include "KismetPins/SGraphPinExec.h"
#include "Playback/Graph/AvaPlaybackGraphSchema.h"
#include "Playback/Graph/Nodes/AvaPlaybackGraphNode.h"
#include "Widgets/SBoxPanel.h"

#define LOCTEXT_NAMESPACE "SAvaPlaybackGraphNode"

void SAvaPlaybackGraphNode::Construct(const FArguments& InArgs, UAvaPlaybackGraphNode* InGraphNode)
{
	PlaybackGraphNode = InGraphNode;
	GraphNode = InGraphNode;
	
	SetCursor(EMouseCursor::CardinalCross);
	UpdateGraphNode();
	
	PostConstruct();
}

void SAvaPlaybackGraphNode::CreateOutputSideAddButton(TSharedPtr<SVerticalBox> OutputBox)
{
	TSharedRef<SWidget> AddPinButton = AddPinButtonContent(LOCTEXT("AvaPlaybackNodeAddPinButton", "Add Input")
		, LOCTEXT("AvaPlaybackNodeAddPinButton_Tooltip", "Adds an input to the Playback node"));

	FMargin AddPinPadding = Settings->GetOutputPinPadding();
	AddPinPadding.Top += 6.0f;

	OutputBox->AddSlot()
		.AutoHeight()
		.VAlign(VAlign_Center)
		.Padding(AddPinPadding)
		[
			AddPinButton
		];
}

EVisibility SAvaPlaybackGraphNode::IsAddPinButtonVisible() const
{
	EVisibility ButtonVisibility = SGraphNode::IsAddPinButtonVisible();
	if (ButtonVisibility == EVisibility::Visible)
	{
		if (PlaybackGraphNode.IsValid() && !PlaybackGraphNode->CanAddInputPin())
		{
			ButtonVisibility = EVisibility::Collapsed;
		}
	}
	return ButtonVisibility;
}

FReply SAvaPlaybackGraphNode::OnAddPin()
{
	if (PlaybackGraphNode.IsValid())
	{
		PlaybackGraphNode->AddInputPin();
		return FReply::Handled();
	}
	
	return FReply::Unhandled();
}

TSharedPtr<SGraphPin> SAvaPlaybackGraphNode::CreatePinWidget(UEdGraphPin* Pin) const
{
	if (Pin->PinType.PinCategory == UAvaPlaybackGraphSchema::PC_Event)
	{
		return SNew(SGraphPinExec, Pin);
	}
	return SGraphNode::CreatePinWidget(Pin);
}

#undef LOCTEXT_NAMESPACE 
