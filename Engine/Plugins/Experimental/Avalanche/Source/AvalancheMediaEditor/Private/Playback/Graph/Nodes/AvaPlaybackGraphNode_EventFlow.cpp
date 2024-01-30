// Copyright Epic Games, Inc. All Rights Reserved.

#include "Playback/Graph/Nodes/AvaPlaybackGraphNode_EventFlow.h"
#include "Playback/Graph/AvaPlaybackGraphSchema.h"
#include "Playback/Nodes/Events/AvaPlaybackNodeFlow.h"

TSubclassOf<UAvaPlaybackNode> UAvaPlaybackGraphNode_EventFlow::GetPlaybackNodeClass() const
{
	return UAvaPlaybackNodeFlow::StaticClass();
}

FName UAvaPlaybackGraphNode_EventFlow::GetInputPinCategory(int32 InputPinIndex) const
{
	return UAvaPlaybackGraphSchema::PC_Event;
}

FName UAvaPlaybackGraphNode_EventFlow::GetOutputPinCategory() const
{
	return UAvaPlaybackGraphSchema::PC_Event;
}
