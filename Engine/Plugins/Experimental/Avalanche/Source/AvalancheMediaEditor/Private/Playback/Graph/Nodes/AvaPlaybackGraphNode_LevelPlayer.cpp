// Copyright Epic Games, Inc. All Rights Reserved.

#include "Playback/Graph/Nodes/AvaPlaybackGraphNode_LevelPlayer.h"
#include "AvalancheMediaEditorSettings.h"
#include "Playback/Graph/AvaPlaybackGraphSchema.h"
#include "Playback/Nodes/AvaPlaybackNodeLevelPlayer.h"
#include "Slate/SAvaPlaybackGraphNode_Player.h"

TSubclassOf<UAvaPlaybackNode> UAvaPlaybackGraphNode_LevelPlayer::GetPlaybackNodeClass() const
{
	return UAvaPlaybackNodeLevelPlayer::StaticClass();
}

FName UAvaPlaybackGraphNode_LevelPlayer::GetInputPinCategory(int32 InputPinIndex) const
{
	return UAvaPlaybackGraphSchema::PC_Event;
}

FLinearColor UAvaPlaybackGraphNode_LevelPlayer::GetNodeTitleColor() const
{
	return UAvalancheMediaEditorSettings::Get().PlaybackPlayerNodeColor;
}

TSharedPtr<SGraphNode> UAvaPlaybackGraphNode_LevelPlayer::CreateVisualWidget()
{
	return SNew(SAvaPlaybackGraphNode_Player, this);
}
