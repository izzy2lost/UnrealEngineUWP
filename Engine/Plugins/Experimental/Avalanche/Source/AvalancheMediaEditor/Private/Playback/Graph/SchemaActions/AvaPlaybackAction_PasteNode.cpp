// Copyright Epic Games, Inc. All Rights Reserved.

#include "Playback/Graph/SchemaActions/AvaPlaybackAction_PasteNode.h"
#include "Playback/AvalanchePlayback.h"
#include "Playback/Graph/AvaPlaybackGraph.h"
#include "Playback/IAvaPlaybackEditor.h"

UEdGraphNode* FAvaPlaybackAction_PasteNode::PerformAction(UEdGraph* ParentGraph, UEdGraphPin* FromPin
	, const FVector2D Location, bool bSelectNewNode)
{
	UAvalanchePlayback* const Playback = CastChecked<UAvaPlaybackGraph>(ParentGraph)->GetPlayback();
	check(Playback);
	
	if (TSharedPtr<IAvaPlaybackGraphEditor> GraphEditor = Playback->GetGraphEditor())
	{
		GraphEditor->PasteNodesHere(Location);
	}
	
	return nullptr;
}
