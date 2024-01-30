// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaPlaybackNode_PlayAnim.h"
#include "Async/Async.h"
#include "AvaBlueprint.h"
#include "AvaScene.h"
#include "AvalancheBroadcast.h"
#include "Engine/Level.h"
#include "Playback/AvalanchePlayback.h"
#include "Playback/Nodes/AvaPlaybackNodeBlueprintPlayer.h"
#include "Playback/Nodes/AvaPlaybackNodeLevelPlayer.h"
#include "AvaSequence.h"

#define LOCTEXT_NAMESPACE "AvalanchePlayback"

namespace Avalanche
{
	FName GetSequenceName(const TObjectPtr<UAvaSequence>& InSequence)
	{
		return InSequence->GetFName();
	}
}

FText UAvaPlaybackNode_PlayAnim::GetNodeDisplayNameText() const
{
	FFormatNamedArguments Args;
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		Args.Add("AnimationTree", FText::GetEmpty());
	}
	else
	{
		FString AnimationTree;
		for (TMap<FSoftObjectPath, FAvalancheAnimations>::TConstIterator Iter(AnimationMap); Iter; ++Iter)
		{
			bool bAction = false;
			for (const FAnimPlaySettings& PlaySettings : Iter->Value.AvailableAnimations)
			{
				if (PlaySettings.Action != EAvaMediaAnimAction::None)
				{
					bAction = true;
					break;
				}
			}
			
			if (bAction)
			{
				AnimationTree.Append(TEXT("\n") + Iter->Key.GetAssetName());
			}
		}		
		Args.Add("AnimationTree", FText::FromString(AnimationTree));
	}
	return FText::Format(LOCTEXT("PlayAnimNode_Title", "Play Animation{AnimationTree}"), Args);
}

FText UAvaPlaybackNode_PlayAnim::GetNodeTooltipText() const
{
	return LOCTEXT("PlayAnimNode_Tooltip", "Plays an Animation from the Avalanche Asset");
}

void UAvaPlaybackNode_PlayAnim::OnEventTriggered(const FAvaPlaybackEventParameters& InEventParameters)
{
	UAvalanchePlayback* const Playback = GetPlayback();
	if (!Playback)
	{
		return;
	}

	const FSoftObjectPath& AvalancheAssetPath = InEventParameters.AvalancheAsset.ToSoftObjectPath();
	TArray<FName> ChannelNames = Playback->GetChannelNamesForIndices(InEventParameters.ChannelIndices);
	
	// Gather the Animations to Play
	if (FAvalancheAnimations* const FoundAnimations = AnimationMap.Find(AvalancheAssetPath))
	{
		for (const FAnimPlaySettings& PlaySettings : FoundAnimations->AvailableAnimations)
		{
			if (PlaySettings.Action != EAvaMediaAnimAction::None)
			{
				for (const FName& ChannelName : ChannelNames)
				{
					Playback->PushAnimationCommand(AvalancheAssetPath, ChannelName.ToString(), PlaySettings.Action, PlaySettings);
				}
			}
		}
	}
}

void UAvaPlaybackNode_PlayAnim::PreDryRun()
{
	SeenAssetsInDryRun.Empty();
}

void UAvaPlaybackNode_PlayAnim::DryRun(const TArray<UAvaPlaybackNode*>& InAncestors)
{
	for (UAvaPlaybackNode* const PlaybackNode : InAncestors)
	{
		if (UAvaPlaybackNodeBlueprintPlayer* const PlayerNode = Cast<UAvaPlaybackNodeBlueprintPlayer>(PlaybackNode))
		{
			TSoftObjectPtr<UAvalancheBlueprint> AvalancheAsset = PlayerNode->GetAvalancheAsset();
			if (UAvalancheBlueprint* const AvalancheBlueprint = AvalancheAsset.LoadSynchronous())
			{
				SeenAssetsInDryRun.Add(PlayerNode->GetAvalancheAssetPath());
				FAvalancheAnimations& Animations = AnimationMap.FindOrAdd(PlayerNode->GetAvalancheAssetPath());
				for (const TObjectPtr<UAvaSequence>& Animation : AvalancheBlueprint->GetSequences())
				{
					if (Animation)
					{
						Animations.AvailableAnimations.FindOrAdd(Avalanche::GetSequenceName(Animation));
					}
				}
			}
		}
		if (UAvaPlaybackNodeLevelPlayer* const PlayerNode = Cast<UAvaPlaybackNodeLevelPlayer>(PlaybackNode))
		{
			TSoftObjectPtr<UWorld> AvalancheAsset = PlayerNode->GetAvalancheAsset();
			if (UWorld* const AvalancheWorld = AvalancheAsset.LoadSynchronous())
			{
				SeenAssetsInDryRun.Add(PlayerNode->GetAvalancheAssetPath());
				FAvalancheAnimations& Animations = AnimationMap.FindOrAdd(PlayerNode->GetAvalancheAssetPath());

				AAvaScene* Scene = nullptr;
				AvalancheWorld->PersistentLevel->Actors.FindItemByClass(&Scene);
				if (IsValid(Scene))
				{
					for (const TObjectPtr<UAvaSequence>& Animation : Scene->GetSequences())
					{
						if (Animation)
						{
							Animations.AvailableAnimations.FindOrAdd(Avalanche::GetSequenceName(Animation));
						}
					}
				}
			}
		}
	}
}

void UAvaPlaybackNode_PlayAnim::PostDryRun()
{
	bool bRefreshNode = false;
	
	//Remove all the Assets that were not Seen.
	for (TMap<FSoftObjectPath, FAvalancheAnimations>::TIterator Iter(AnimationMap); Iter; ++Iter)
	{
		if (!SeenAssetsInDryRun.Contains(Iter->Key))
		{
			Iter.RemoveCurrent();
			bRefreshNode = true;
		}
	}

	if (bRefreshNode)
	{
		RefreshNode(false);
	}

	SeenAssetsInDryRun.Empty();
}

#undef LOCTEXT_NAMESPACE
