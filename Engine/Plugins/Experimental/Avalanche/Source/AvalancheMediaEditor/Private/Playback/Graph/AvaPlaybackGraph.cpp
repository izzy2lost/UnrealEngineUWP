// Copyright Epic Games, Inc. All Rights Reserved.

#include "Playback/Graph/AvaPlaybackGraph.h"
#include "Playback/AvalanchePlayback.h"
#include "Playback/Graph/Nodes/AvaPlaybackGraphNode.h"

UAvalanchePlayback* UAvaPlaybackGraph::GetPlayback() const
{
	return CastChecked<UAvalanchePlayback>(GetOuter());
}

UAvaPlaybackGraphNode* UAvaPlaybackGraph::CreatePlaybackGraphNode(TSubclassOf<UAvaPlaybackGraphNode> NewNodeClass, bool bSelectNewNode)
{
	return CastChecked<UAvaPlaybackGraphNode>(CreateNode(NewNodeClass, false, bSelectNewNode));
}
