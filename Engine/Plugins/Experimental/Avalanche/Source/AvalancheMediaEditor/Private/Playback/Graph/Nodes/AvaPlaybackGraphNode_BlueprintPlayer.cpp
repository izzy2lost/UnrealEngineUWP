// Copyright Epic Games, Inc. All Rights Reserved.

#include "Playback/Graph/Nodes/AvaPlaybackGraphNode_BlueprintPlayer.h"
#include "AvalancheMediaEditorSettings.h"
#include "Playback/Graph/AvaPlaybackGraphSchema.h"
#include "Playback/Nodes/AvaPlaybackNodeBlueprintPlayer.h"
#include "Slate/SAvaPlaybackGraphNode_Player.h"

TSubclassOf<UAvaPlaybackNode> UAvaPlaybackGraphNode_BlueprintPlayer::GetPlaybackNodeClass() const
{
	return UAvaPlaybackNodeBlueprintPlayer::StaticClass();
}

FName UAvaPlaybackGraphNode_BlueprintPlayer::GetInputPinCategory(int32 InputPinIndex) const
{
	return UAvaPlaybackGraphSchema::PC_Event;
}

FLinearColor UAvaPlaybackGraphNode_BlueprintPlayer::GetNodeTitleColor() const
{
	return UAvalancheMediaEditorSettings::Get().PlaybackPlayerNodeColor;
}

TSharedPtr<SGraphNode> UAvaPlaybackGraphNode_BlueprintPlayer::CreateVisualWidget()
{
	return SNew(SAvaPlaybackGraphNode_Player, this);
}
