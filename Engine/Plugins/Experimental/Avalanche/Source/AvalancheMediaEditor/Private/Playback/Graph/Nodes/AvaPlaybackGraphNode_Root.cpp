// Copyright Epic Games, Inc. All Rights Reserved.

#include "Playback/Graph/Nodes/AvaPlaybackGraphNode_Root.h"
#include "AvalancheMediaEditorSettings.h"
#include "Playback/Nodes/AvaPlaybackNodeRoot.h"
#include "Slate/SAvaPlaybackGraphNode_Root.h"

#define LOCTEXT_NAMESPACE "AvaPlaybackGraphNode_Root"

TSubclassOf<UAvaPlaybackNode> UAvaPlaybackGraphNode_Root::GetPlaybackNodeClass() const
{
	return UAvaPlaybackNodeRoot::StaticClass();
}

FLinearColor UAvaPlaybackGraphNode_Root::GetNodeTitleColor() const
{
	return UAvalancheMediaEditorSettings::Get().PlaybackChannelsNodeColor;
}

bool UAvaPlaybackGraphNode_Root::CanUserDeleteNode() const
{
	return false;
}

bool UAvaPlaybackGraphNode_Root::CanDuplicateNode() const
{
	return false;
}

TSharedPtr<SGraphNode> UAvaPlaybackGraphNode_Root::CreateVisualWidget()
{
	return SNew(SAvaPlaybackGraphNode_Root, this);
}

#undef LOCTEXT_NAMESPACE
