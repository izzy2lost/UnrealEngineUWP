// Copyright Epic Games, Inc. All Rights Reserved.

#include "Playlist/AvaPlaylistPlaybackClientWatcher.h"

#include "AvalancheBroadcast.h"
#include "Playback/IAvaMediaPlaybackClient.h"
#include "Playlist/AvalanchePlaylist.h"

FAvaPlaylistPlaybackClientWatcher::FAvaPlaylistPlaybackClientWatcher(UAvalanchePlaylist* InPlaylist)
	: Playlist(InPlaylist)
{
	using namespace UE::AvaMediaPlaybackClient::Delegates;
	GetOnPlaybackStatusChanged().AddRaw(this, &FAvaPlaylistPlaybackClientWatcher::HandlePlaybackStatusChanged);
	
}
FAvaPlaylistPlaybackClientWatcher::~FAvaPlaylistPlaybackClientWatcher()
{
	using namespace UE::AvaMediaPlaybackClient::Delegates;
	GetOnPlaybackStatusChanged().RemoveAll(this);
}

void FAvaPlaylistPlaybackClientWatcher::TryRestorePlaySubPage(int InPageId, const UE::AvaMediaPlaybackClient::Delegates::FPlaybackStatusChangedArgs& InEventArgs) const
{
	// Restore page player and local playback proxies.
	// (Need to specify the InstanceId from the server for everything to match.)
	const FName ChannelFName(InEventArgs.ChannelName);

	// Ensure the specified channel exists.
	if (UAvalancheBroadcast::Get().GetChannelIndex(ChannelFName) == INDEX_NONE)
	{
		UE_LOG(LogAvaPlaylist, Error,
			TEXT("Received a playback object on channel \"%s\" which doesn't exist locally. Playback Server should be reset."),
			*InEventArgs.ChannelName);
		return;
	}
	
	const bool bIsPreview = UAvalancheBroadcast::Get().GetChannelType(ChannelFName) == EAvaBroadcastChannelType::Preview ? true : false;

	const FAvalanchePage& PageToRestore = Playlist->GetPage(InPageId);

	const int32 SubPageIndex = PageToRestore.GetAvalancheAssetPaths(Playlist).Find(InEventArgs.AssetPath);

	if (SubPageIndex == INDEX_NONE)
	{
		UE_LOG(LogAvaPlaylist, Error,
			TEXT("Asset mismatch (expected (any of): \"%s\", received: \"%s\") for restoring page %d. Playback Server should be reset."),
			*FString::JoinBy(PageToRestore.GetAvalancheAssetPaths(Playlist), TEXT(","), [](const FSoftObjectPath& Path){ return Path.ToString();}),
			*InEventArgs.AssetPath.ToString(), InPageId);
		return;
	}

	if (!Playlist->RestorePlaySubPage(InPageId, SubPageIndex, InEventArgs.InstanceId, bIsPreview, ChannelFName))
	{
		UE_LOG(LogAvaPlaylist, Error, TEXT("Failed to restore page %d. Playback Server should be reset."), InPageId);
	}
}

void FAvaPlaylistPlaybackClientWatcher::HandlePlaybackStatusChanged(IAvaMediaPlaybackClient& InPlaybackClient,
	const UE::AvaMediaPlaybackClient::Delegates::FPlaybackStatusChangedArgs& InEventArgs)
{
	if (!Playlist)
	{
		return;
	}

	static const TArray<EAvaMediaPlaybackStatus> RunningStates =
	{
		//EAvaMediaPlaybackStatus::Starting, // Starting is not reliable, it may also mean "loading". FIXME.
		EAvaMediaPlaybackStatus::Started
	};

	// Try to determine if a playback has started or stopped.
	const bool bWasRunning = IsAnyOf(InEventArgs.PrevStatus, RunningStates);
	const bool bIsRunning = IsAnyOf(InEventArgs.NewStatus, RunningStates);
	
	// If a playback instance is stopping, stop corresponding page (if any).
	if (bWasRunning && !bIsRunning)
	{
		for (UAvalanchePagePlayer* PagePlayer : Playlist->PagePlayers)
		{
			// Search for a match with the event:				
			if (PagePlayer && PagePlayer->ChannelName == InEventArgs.ChannelName)
			{
				PagePlayer->ForEachInstancePlayer([&InEventArgs](UAvaRundownPlaybackInstancePlayer* InInstancePlayer)
				{
					if (InInstancePlayer
						&& InInstancePlayer->SourceAssetPath == InEventArgs.AssetPath
						&& InInstancePlayer->GetPlaybackInstanceId() != InEventArgs.InstanceId)
					{
						InInstancePlayer->Stop();	
					}
				});

				// If we stopped all the instance players, stop the page (to broadcast events).
				if (!PagePlayer->IsPlaying())
				{
					PagePlayer->Stop();	
				}
			}
		}
		Playlist->RemoveStoppedPagePlayers();
	}

	// Note: execute this even if not on rising transition because it may be a user data update following the "GetUserData" request.
	if (bIsRunning)
	{
		// We need to figure out which page it is.
		const FString* UserData = InPlaybackClient.GetRemotePlaybackUserData(InEventArgs.InstanceId, InEventArgs.AssetPath, InEventArgs.ChannelName);

		// We haven't received the user data for this playback. So we request it.
		// This event will be received again with user data next time.
		if (!UserData)
		{
			InPlaybackClient.RequestPlayback(InEventArgs.InstanceId, InEventArgs.AssetPath, InEventArgs.ChannelName, EAvaMediaPlaybackAction::GetUserData);
		}
		else
		{
			const int32 PageId = UAvalanchePagePlayer::GetPageIdFromInstanceUserData(*UserData);
			if (PageId != FAvalanchePage::InvalidPageId)
			{
				const UAvalanchePagePlayer* PagePlayer = Playlist->FindPlayerForProgramPage(PageId);
				if (!PagePlayer || !PagePlayer->FindInstancePlayerByInstanceId(InEventArgs.InstanceId))
				{
					TryRestorePlaySubPage(PageId, InEventArgs);
				}
			}
		}
	}
}