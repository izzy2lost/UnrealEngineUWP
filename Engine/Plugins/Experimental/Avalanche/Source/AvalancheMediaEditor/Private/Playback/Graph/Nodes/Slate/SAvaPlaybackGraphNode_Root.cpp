// Copyright Epic Games, Inc. All Rights Reserved.

#include "SAvaPlaybackGraphNode_Root.h"
#include "Playback/Graph/AvaPlaybackGraphSchema.h"
#include "Playback/Graph/Pins/Slate/SAvaPlaybackGraphPin_Channel.h"

TSharedPtr<SGraphPin> SAvaPlaybackGraphNode_Root::CreatePinWidget(UEdGraphPin* Pin) const
{
	if (Pin->PinType.PinCategory == UAvaPlaybackGraphSchema::PC_ChannelFeed)
	{
		return SNew(SAvaPlaybackGraphPin_Channel, Pin);
	}
	return SGraphNode::CreatePinWidget(Pin);
}
