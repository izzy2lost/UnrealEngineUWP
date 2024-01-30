// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaMediaDefines.h"
#include "Playback/AvaMediaPlaybackClientDelegates.h"

class IAvaMediaPlaybackClient;
class UAvalanchePlaylist;

/**
 * Playback Client Watcher ensures external playback events are reconciled.
 * Cases covered:
 * 1 - if a remote playable is stopped (server reset), the corresponding page in the playlist will be stopped
 * 2 - if a remote playable is playing (server side), the corresponding page's state will be restored, i.e. rebuilding the proxies.
 * to be continued...
 */
class FAvaPlaylistPlaybackClientWatcher
{
public:
	FAvaPlaylistPlaybackClientWatcher(UAvalanchePlaylist* InPlaylist);
	~FAvaPlaylistPlaybackClientWatcher();

	void TryRestorePlaySubPage(int InPageId, const UE::AvaMediaPlaybackClient::Delegates::FPlaybackStatusChangedArgs& InEventArgs) const;

	static bool IsAnyOf(EAvaMediaPlaybackStatus InStatus, const TArray<EAvaMediaPlaybackStatus>& InStatuses)
	{
		for (const EAvaMediaPlaybackStatus Status : InStatuses)
		{
			if (Status == InStatus)
			{
				return true;
			}
		}
		return false;
	}
	
	void HandlePlaybackStatusChanged(IAvaMediaPlaybackClient& InPlaybackClient,
		const UE::AvaMediaPlaybackClient::Delegates::FPlaybackStatusChangedArgs& InEventArgs);
	
private:
	UAvalanchePlaylist* Playlist;
};
