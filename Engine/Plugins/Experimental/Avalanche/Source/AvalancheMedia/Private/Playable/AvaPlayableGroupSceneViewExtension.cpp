// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaPlayableGroupSceneViewExtension.h"

#include "AvaPlayableGroupManager.h"
#include "AvaPlayableGroupSubsystem.h"
#include "Engine/GameInstance.h"
#include "IAvaMediaModule.h"
#include "Playable/AvaPlayableGroup.h"
#include "Playback/AvaPlaybackManager.h"
#include "Playback/IAvaPlaybackServer.h"
#include "SceneView.h"

FAvaPlayableGroupSceneViewExtension::FAvaPlayableGroupSceneViewExtension(const FAutoRegister& InAutoReg)
	: FSceneViewExtensionBase(InAutoReg)
{
}

void FAvaPlayableGroupSceneViewExtension::SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView)
{
	if (!InViewFamily.Scene)
	{
		return;
	}
	
	const UWorld* ViewWorld = InViewFamily.Scene->GetWorld();
	if (!ViewWorld)
	{
		return;
	}

	// Ideally, we would like a direct link from GameInstance (or World) to it's owning PlayableGroup and PlayableGroupManager.
	// Todo: Refactor the AvaGameInstance path to have a link to PlayableGroup.
	const UAvaPlayableGroupManager* PlayableGroupManager = nullptr;

	// For Game Viewport output, the sub system will give us the playable group manager directly.
	if (const UGameInstance* GameInstance = ViewWorld->GetGameInstance())
	{
		if (const UAvaPlayableGroupSubsystem* PlayableGroupSubsystem = GameInstance->GetSubsystem<UAvaPlayableGroupSubsystem>())
		{
			PlayableGroupManager = PlayableGroupSubsystem->PlayableGroupManager;
		}
	}

	if (!PlayableGroupManager)
	{
		const FAvaPlaybackManager& PlaybackManager = IAvaMediaModule::Get().GetLocalPlaybackManager();
		PlayableGroupManager = PlaybackManager.GetPlayableGroupManager();
	}

	// Search in the local playable group manager for that world.
	UAvaPlayableGroup* ViewPlayableGroup = PlayableGroupManager->FindPlayableGroupForWorld(ViewWorld);
	
	// If not found, search in the playback server's playback manager.
	if (!ViewPlayableGroup && IAvaMediaModule::Get().IsPlaybackServerStarted())
	{
		PlayableGroupManager = IAvaMediaModule::Get().GetPlaybackServer()->GetPlaybackManager().GetPlayableGroupManager();
		ViewPlayableGroup = PlayableGroupManager->FindPlayableGroupForWorld(ViewWorld);
	}

	if (ViewPlayableGroup)
	{
		ViewPlayableGroup->SetupView(InViewFamily, InView);
	}
}