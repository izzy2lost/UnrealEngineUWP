// Copyright Epic Games, Inc. All Rights Reserved.

#include "Playback/Graph/Nodes/AvaPlaybackGraphNode_EventTrigger.h"
#include "AvalancheMediaEditorSettings.h"
#include "Playback/Graph/AvaPlaybackGraphSchema.h"
#include "Playback/Nodes/Events/AvaPlaybackNodeTrigger.h"

TSubclassOf<UAvaPlaybackNode> UAvaPlaybackGraphNode_EventTrigger::GetPlaybackNodeClass() const
{
	return UAvaPlaybackNodeTrigger::StaticClass();
}

FName UAvaPlaybackGraphNode_EventTrigger::GetOutputPinCategory() const
{
	return UAvaPlaybackGraphSchema::PC_Event;
}

FLinearColor UAvaPlaybackGraphNode_EventTrigger::GetNodeTitleColor() const
{
	return UAvalancheMediaEditorSettings::Get().PlaybackEventNodeColor;
}
