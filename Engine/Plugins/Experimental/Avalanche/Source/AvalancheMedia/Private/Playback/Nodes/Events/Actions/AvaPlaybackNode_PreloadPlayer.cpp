// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaPlaybackNode_PreloadPlayer.h"
#include "AvalancheBroadcast.h"
#include "Playback/AvalanchePlayback.h"

#define LOCTEXT_NAMESPACE "AvalanchePlayback"

FText UAvaPlaybackNode_PreloadPlayer::GetNodeDisplayNameText() const
{
	return LOCTEXT("PreloadPlayerNode_Title", "Preload Player");
}

FText UAvaPlaybackNode_PreloadPlayer::GetNodeTooltipText() const
{
	return LOCTEXT("PreloadPlayerNode_Tooltip", "Loads the Motion Design World Preemptively and Begins Play on that World");
}

void UAvaPlaybackNode_PreloadPlayer::OnEventTriggered(const FAvaPlaybackEventParameters& InEventParameters)
{
	UAvalanchePlayback* const Playback = GetPlayback();
	
	//Make sure we have a valid Avalanche Asset
	if (Playback && InEventParameters.IsAssetValid())
	{
		TArray<FName> ChannelNames = Playback->GetChannelNamesForIndices(InEventParameters.ChannelIndices);
		for (const FName& ChannelName : ChannelNames)
		{
			Playback->LoadAsset(InEventParameters.AvalancheAsset, ChannelName);
		}
	}
}

#undef LOCTEXT_NAMESPACE 