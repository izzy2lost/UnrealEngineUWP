// Copyright Epic Games, Inc. All Rights Reserved.

#include "Playback/Nodes/AvaPlaybackNodePlayer.h"

#include "AvalancheBroadcast.h"
#include "Playback/AvalanchePlayable.h"
#include "Playback/AvalanchePlayback.h"
#include "Playback/Nodes/Events/AvaPlaybackNodeEvent.h"

#define LOCTEXT_NAMESPACE "AvalanchePlayback"

UAvaPlaybackNodePlayer::UAvaPlaybackNodePlayer() = default;

void UAvaPlaybackNodePlayer::PostAllocateNode()
{
	Super::PostAllocateNode();
	if (UAvalanchePlayback* const Playback = GetPlayback())
	{
		Playback->AddPlayerNode(this);
	}
}

void UAvaPlaybackNodePlayer::Tick(float DeltaTime, FAvaPlaybackChannelParameters& ChannelParameters)
{
	ChannelParameters.AvalancheAssets.AddUnique(GetAvalancheAssetPtr());
	ChannelIndices.Add(ChannelParameters.ChannelIndex);
	//No Children Ticks. The Player Node is the Dead End for Ticking.
	//Events are handled separately
}

void UAvaPlaybackNodePlayer::TickEventFeed(float DeltaTime)
{
	for (int32 Index = 0; Index < ChildNodes.Num(); ++Index)
	{
		//TODO: Should we cache these Event Nodes? 
		if (UAvaPlaybackNodeEvent* const EventNode = Cast<UAvaPlaybackNodeEvent>(ChildNodes[Index]))
		{
			FAvaPlaybackEventParameters EventParameters;
			EventParameters.ChannelIndices = ChannelIndices;
			EventParameters.AvalancheAsset = GetAvalancheAssetPtr();
			
			EventNode->TickEvent(DeltaTime, EventParameters);
			
			if (EventParameters.ShouldTriggerEventAction())
			{
				NotifyChildNodeSucceeded(Index);
			}
		}
	}
}

void UAvaPlaybackNodePlayer::ResetEvents()
{
	// Note: Keep the channel indices from last tick around so the preview knows which
	// channel to fetch. This happens outside of the playback tick so the ChannelIndices
	// would be reset at that point (see GetPreviewRenderTarget, called from slate UI update).
	LastTickChannelIndices = ChannelIndices;

	ChannelIndices.Reset();
	
	TArray<TObjectPtr<UAvaPlaybackNode>> RemainingNodes = ChildNodes;
	while (!RemainingNodes.IsEmpty())
	{
		//Only iterate Event Nodes (all of Player's child nodes should be node events.)
		if (UAvaPlaybackNodeEvent* const EventNode = Cast<UAvaPlaybackNodeEvent>(RemainingNodes.Pop()))
		{
			EventNode->Reset();
			RemainingNodes.Append(EventNode->GetChildNodes());
		}
	}
}

FText UAvaPlaybackNodePlayer::GetNodeDisplayNameText() const
{
	return DisplayNameText;
}

FText UAvaPlaybackNodePlayer::GetNodeTooltipText() const
{
	return LOCTEXT("PlayerNode_ToolTip", "Plays the given Avalanche Asset");
}

#if WITH_EDITOR

UTextureRenderTarget2D* UAvaPlaybackNodePlayer::GetPreviewRenderTarget() const
{
	// Returns the first valid channel index we have.
	for (const int32 ChannelIndex : LastTickChannelIndices)
	{
		const FName ChannelName = UAvalancheBroadcast::Get().GetChannelName(ChannelIndex);
		if (ChannelName.IsNone())
		{
			continue;
		}

		if (const UAvalanchePlayback* const Playback = GetPlayback())
		{
			if (const UAvalanchePlayable* const Playable = Playback->FindPlayable(GetAvalancheAssetPath(), ChannelName))
			{
				if (const UAvaMediaPlayableGroup* PlayableGroup = Playable->GetPlayableGroup())
				{
					if (UTextureRenderTarget2D* RenderTarget = PlayableGroup->GetRenderTarget())
					{
						return RenderTarget;
					}
				}
			}
		}

		// Fallback to channel RT which is likely to be (actually should be) the same as the game instance.
		const FAvaOutputChannel& Channel = UAvalancheBroadcast::Get().GetCurrentProfile().GetChannel(ChannelName);
		if (Channel.IsValidChannel())
		{
			return Channel.GetCurrentRenderTarget(true);
		}
	}
	return nullptr;
}
#endif

const FSoftObjectPath& UAvaPlaybackNodePlayer::GetAvalancheAssetPath() const
{
	static FSoftObjectPath EmptyPath;
	return EmptyPath;
}

#undef LOCTEXT_NAMESPACE
