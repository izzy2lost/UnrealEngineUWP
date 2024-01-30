// Copyright Epic Games, Inc. All Rights Reserved.

#include "Playback/Graph/Nodes/AvaPlaybackGraphNode_EventAction.h"
#include "AvalancheMediaEditorSettings.h"
#include "Playback/Graph/AvaPlaybackGraphSchema.h"
#include "Playback/Nodes/Events/AvaPlaybackNodeAction.h"

TSubclassOf<UAvaPlaybackNode> UAvaPlaybackGraphNode_EventAction::GetPlaybackNodeClass() const
{
	return UAvaPlaybackNodeAction::StaticClass();
}

FName UAvaPlaybackGraphNode_EventAction::GetInputPinCategory(int32 InputPinIndex) const
{
	return UAvaPlaybackGraphSchema::PC_Event;
}

FName UAvaPlaybackGraphNode_EventAction::GetOutputPinCategory() const
{
	return UAvaPlaybackGraphSchema::PC_Event;
}

FLinearColor UAvaPlaybackGraphNode_EventAction::GetNodeTitleColor() const
{
	return UAvalancheMediaEditorSettings::Get().PlaybackActionNodeColor;
}
