// Copyright Epic Games, Inc. All Rights Reserved.

#include "Playlist/AvalanchePlaylistComponent.h"

#include "Engine/World.h"
#include "IAvaMediaModule.h"
#include "Playback/AvaMediaPlaybackManager.h"
#include "Playlist/AvalanchePlaylist.h"

UAvalanchePlaylistComponent::UAvalanchePlaylistComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bWantsInitializeComponent = true;
}

bool UAvalanchePlaylistComponent::PlayPage(int32 InPageId)
{
	if (Playlist && !Playlist->IsPagePlaying(InPageId))
	{
		return Playlist->PlayPage(InPageId, EAvaPlayType::PlayFromStart);
	}
	return false;
}

bool UAvalanchePlaylistComponent::StopPage(int32 InPageId)
{
	if (Playlist && Playlist->IsPagePlaying(InPageId))
	{
		return Playlist->StopPage(InPageId, EAvaPlaylistPageStopOptions::Default, false);
	}
	return false;
}

int32 UAvalanchePlaylistComponent::GetNumberOfPages() const
{
	return Playlist ? Playlist->GetInstancedPages().Pages.Num() : 0;
}

int32 UAvalanchePlaylistComponent::GetPageIdForIndex(int32 InPageIndex) const
{
	if (Playlist && Playlist->GetInstancedPages().Pages.IsValidIndex(InPageIndex))
	{
		return Playlist->GetInstancedPages().Pages[InPageIndex].GetPageId();
	}
	return 0;
}

void UAvalanchePlaylistComponent::InitializeComponent()
{
	Super::InitializeComponent();
	FWorldDelegates::OnWorldBeginTearDown.AddUObject(this, &UAvalanchePlaylistComponent::OnWorldBeginTearDown);
}

void UAvalanchePlaylistComponent::UninitializeComponent()
{
	FWorldDelegates::OnWorldBeginTearDown.RemoveAll(this);
	Super::UninitializeComponent();
}

void UAvalanchePlaylistComponent::OnWorldBeginTearDown(UWorld* InWorld)
{
	if (InWorld == GetWorld())
	{
		IAvaMediaModule::Get().GetLocalPlaybackManager().OnParentWorldBeginTearDown();
		if (Playlist)
		{
			// Since we have forcibly teared down the manager, update the
			// status of the playlist to reflect the appropriate state.
			Playlist->OnParentWordBeginTearDown();
		}
	}
}

