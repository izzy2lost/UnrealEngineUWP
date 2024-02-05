// Copyright Epic Games, Inc. All Rights Reserved.

#include "Playback/Graph/Nodes/AvaPlaybackEditorGraphNode_BlueprintPlayer.h"

#include "AvaMediaEditorSettings.h"
#include "Playback/Graph/AvaPlaybackEditorGraphSchema.h"
#include "Playback/Nodes/AvaPlaybackNodeBlueprintPlayer.h"
#include "Slate/SAvaPlaybackEditorGraphNode_Player.h"

TSubclassOf<UAvaPlaybackNode> UAvaPlaybackEditorGraphNode_BlueprintPlayer::GetPlaybackNodeClass() const
{
	return UAvaPlaybackNodeBlueprintPlayer::StaticClass();
}

FName UAvaPlaybackEditorGraphNode_BlueprintPlayer::GetInputPinCategory(int32 InputPinIndex) const
{
	return UAvaPlaybackEditorGraphSchema::PC_Event;
}

FLinearColor UAvaPlaybackEditorGraphNode_BlueprintPlayer::GetNodeTitleColor() const
{
	return UAvaMediaEditorSettings::Get().PlaybackPlayerNodeColor;
}

TSharedPtr<SGraphNode> UAvaPlaybackEditorGraphNode_BlueprintPlayer::CreateVisualWidget()
{
	return SNew(SAvaPlaybackEditorGraphNode_Player, this);
}
