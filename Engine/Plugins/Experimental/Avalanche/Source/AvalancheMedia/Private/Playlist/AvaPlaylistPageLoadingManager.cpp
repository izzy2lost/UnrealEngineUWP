// Copyright Epic Games, Inc. All Rights Reserved.

#include "Playlist/AvaPlaylistPageLoadingManager.h"

#include "AvalancheBroadcast.h"
#include "Playback/AvaMediaPlaybackManager.h"

namespace UE::AvaPlaylistPageLoadingManager::Private
{
	bool IsLoaded(EAvaMediaPlaybackStatus InStatus)
	{
		switch (InStatus)
		{
		case EAvaMediaPlaybackStatus::Unknown:
		case EAvaMediaPlaybackStatus::Missing:
		case EAvaMediaPlaybackStatus::Syncing:
		case EAvaMediaPlaybackStatus::Available:
		case EAvaMediaPlaybackStatus::Loading:
			return false;
		case EAvaMediaPlaybackStatus::Loaded:
		case EAvaMediaPlaybackStatus::Starting:
		case EAvaMediaPlaybackStatus::Stopping:
		case EAvaMediaPlaybackStatus::Unloading:
			return true;
		case EAvaMediaPlaybackStatus::Error:
		default:
			return false;
		}
	}

	bool IsError(EAvaMediaPlaybackStatus InStatus)
	{
		return InStatus == EAvaMediaPlaybackStatus::Error;
	}

	bool IsLoading(EAvaMediaPlaybackStatus InStatus)
	{
		return InStatus == EAvaMediaPlaybackStatus::Loading;
	}
}

FAvaPlaylistPageLoadingManager::FAvaPlaylistPageLoadingManager(UAvalanchePlaylist* InPlaylist)
	: Playlist(InPlaylist)
{
	PendingRequestSet.Reserve(32);
	LoadingInstances.Reserve(4);

	if (Playlist)
	{
		Playlist->GetPlaybackManager().OnPlaybackInstanceStatusChanged.AddRaw(this, &FAvaPlaylistPageLoadingManager::HandlePlaybackInstanceStatusChanged);
		Playlist->GetPlaybackManager().OnBeginTick.AddRaw(this, &FAvaPlaylistPageLoadingManager::HandlePlaybackManagerBeginTick);
		PlaybackManagerWeak = Playlist->GetPlaybackManager().AsShared();
	}
}

FAvaPlaylistPageLoadingManager::~FAvaPlaylistPageLoadingManager()
{
	if (const TSharedPtr<FAvaMediaPlaybackManager> PlaybackManager = PlaybackManagerWeak.Pin())
	{
		PlaybackManager->OnPlaybackInstanceStatusChanged.RemoveAll(this);
		PlaybackManager->OnBeginTick.RemoveAll(this);
	}
}

bool FAvaPlaylistPageLoadingManager::RequestLoadPage(int32 InPageId, bool bInIsPreview, const FName& InPreviewChannelName)
{
	using namespace UE::AvaPlaylistPageLoadingManager::Private;
	
	if (!Playlist)
	{
		return false;
	}

	const FAvalanchePage& Page = Playlist->GetPage(InPageId);
	if (!Page.IsValidPage())
	{
		return false;
	}
	
	if (LoadingInstances.Num() < MaxLoadingInstances)
	{
		return RequestLoadPageInternal(Page, bInIsPreview, InPreviewChannelName);
	}

	const FPageLoadRequest Request = {InPageId, bInIsPreview, InPreviewChannelName};

	// Prevent the same request from being added more than once.
	// It may be necessary to do that to prevent request spamming.
	if (!PendingRequestSet.Contains(Request))
	{
		PendingRequestQueue.Enqueue(Request);
		PendingRequestSet.Add(Request);
	}
	else
	{
		UE_LOG(LogAvaPlaylist, Warning,
			TEXT("Page Loading Manager: Request for page id \"%d\" type: \"%s\" Channel \"%s\" is already in the queue."),
			InPageId, bInIsPreview ? TEXT("Preview") : TEXT("Program"), *InPreviewChannelName.ToString());
	}
	return true;
}

bool FAvaPlaylistPageLoadingManager::RequestLoadPageInternal(const FAvalanchePage& InPage, bool bInIsPreview, const FName& InPreviewChannelName)
{
	using namespace UE::AvaPlaylistPageLoadingManager::Private;
	
	const TArray<UAvalanchePlaylist::FLoadedInstanceInfo> LoadedInstances = Playlist->LoadPage(InPage.GetPageId(), bInIsPreview, InPreviewChannelName);
	
	if (LoadedInstances.IsEmpty())
	{
		return false;
	}
	
	for (const UAvalanchePlaylist::FLoadedInstanceInfo& LoadedInstance : LoadedInstances)
	{
		FInstanceInfo InstanceInfo;
		InstanceInfo.InstanceId = LoadedInstance.InstanceId;
		InstanceInfo.ChannelName = bInIsPreview ? InPreviewChannelName.ToString() : InPage.GetChannelName().ToString();
		InstanceInfo.AssetPath = LoadedInstance.AssetPath;
		
		// Check if the instance is already loaded
		const TSharedPtr<FAvaMediaPlaybackInstance> PlaybackInstance = FindInstance(InstanceInfo);

		if (!PlaybackInstance || IsError(PlaybackInstance->GetStatus()))
		{
			continue;
		}

		// If it is not loaded yet, wait for it.
		if (!IsLoaded(PlaybackInstance->GetStatus()))
		{
			// Keep track of the current status so we can better track what is going on.
			InstanceInfo.Status = PlaybackInstance->GetStatus();
			// Keep track of submit time so we can time it out if it takes too long.
			InstanceInfo.SubmitTime = FDateTime::UtcNow();
			LoadingInstances.Add(InstanceInfo);
		}
	}
	
	return !LoadedInstances.IsEmpty();
}

void FAvaPlaylistPageLoadingManager::HandlePlaybackInstanceStatusChanged(const FAvaMediaPlaybackInstance& InPlaybackInstance)
{
	using namespace UE::AvaPlaylistPageLoadingManager::Private;
	for (TArray<FInstanceInfo>::TIterator InstanceIterator = LoadingInstances.CreateIterator(); InstanceIterator; ++InstanceIterator  )
	{
		FInstanceInfo& InstanceInfo = *InstanceIterator;
		if (InstanceInfo.InstanceId == InPlaybackInstance.GetInstanceId())
		{
			if (IsError(InPlaybackInstance.GetStatus()) || IsLoaded(InPlaybackInstance.GetStatus()))
			{
				InstanceIterator.RemoveCurrent();
			}
		}
	}
}

void FAvaPlaylistPageLoadingManager::HandlePlaybackManagerBeginTick(float InDeltaSeconds)
{
	using namespace UE::AvaPlaylistPageLoadingManager::Private;

	const FDateTime CurrentTime = FDateTime::UtcNow();
	const FTimespan TimeoutSpan = FTimespan::FromSeconds(2);

	// Attempt to prune the waiting list.
	for (TArray<FInstanceInfo>::TIterator InstanceIterator = LoadingInstances.CreateIterator(); InstanceIterator; ++InstanceIterator  )
	{
		FInstanceInfo& InstanceInfo = *InstanceIterator;
		const TSharedPtr<FAvaMediaPlaybackInstance> PlaybackInstance = FindInstance(InstanceInfo);

		if (!PlaybackInstance)
		{
			UE_LOG(LogAvaPlaylist, Error,
				TEXT("Page Loading Manager: Failed to find playback instance Id \"%s\" Path \"%s\" Channel \"%s\"."),
				*InstanceInfo.InstanceId.ToString(), *InstanceInfo.AssetPath.ToString(), *InstanceInfo.ChannelName);
			
			InstanceIterator.RemoveCurrent();
			continue;
		}

		if (IsError(PlaybackInstance->GetStatus()))
		{
			UE_LOG(LogAvaPlaylist, Error,
				TEXT("Page Loading Manager: Failed to load playback instance Id \"%s\" Path \"%s\" Channel \"%s\"."),
				*InstanceInfo.InstanceId.ToString(), *InstanceInfo.AssetPath.ToString(), *InstanceInfo.ChannelName);
			InstanceIterator.RemoveCurrent();
			continue;
		}

		// Success condition
		if (IsLoaded(PlaybackInstance->GetStatus()))
		{
			InstanceIterator.RemoveCurrent();
			continue;
		}

		// Handle an edge case:
		// for the remote server, the load command might have been lost. If the playback instance returns to a status
		// of "Available", something went wrong and the asset loading has been interrupted. Server went offline or
		// was externally reset.
		if (IsLoading(InstanceInfo.Status) && !IsLoading(PlaybackInstance->GetStatus()))
		{
			UE_LOG(LogAvaPlaylist, Warning,
				TEXT("Page Loading Manager: Playback instance Id \"%s\" Path \"%s\" Channel \"%s\" stopped loading."),
					*InstanceInfo.InstanceId.ToString(), *InstanceInfo.AssetPath.ToString(), *InstanceInfo.ChannelName);
			// TODO: special recovery? Might push the request again back in the queue and try again.
			InstanceIterator.RemoveCurrent();
			continue;
		}

		// Finally check for timeout.
		// Remark: load time may be super long for things that need to import animated skeletal mesh (alembic) those will timeout for sure.
		// If a load request times out, we just keep going and request the next page, it will finish eventually.
		if (CurrentTime - InstanceInfo.SubmitTime > TimeoutSpan)
		{
			UE_LOG(LogAvaPlaylist, Warning,
				TEXT("Page Loading Manager: Playback instance Id \"%s\" Path \"%s\" Channel \"%s\" loading request timed out."),
					*InstanceInfo.InstanceId.ToString(), *InstanceInfo.AssetPath.ToString(), *InstanceInfo.ChannelName);
			InstanceIterator.RemoveCurrent();
		}

		// Update status so we can detect deltas.
		// This would normally be detected and propagated as status change event in playback manager.
		InstanceInfo.Status = PlaybackInstance->GetStatus();
	}

	if (Playlist)
	{
		while (LoadingInstances.Num() < MaxLoadingInstances && !PendingRequestQueue.IsEmpty())
		{
			FPageLoadRequest Request;
			if (PendingRequestQueue.Dequeue(Request))
			{
				PendingRequestSet.Remove(Request);
				const FAvalanchePage& Page = Playlist->GetPage(Request.PageId);
				if (Page.IsValidPage())
				{
					RequestLoadPageInternal(Page, Request.bIsPreview, Request.PreviewChannelName);
				}
			}
		}
	}
}

TSharedPtr<FAvaMediaPlaybackInstance> FAvaPlaylistPageLoadingManager::FindInstance(const FInstanceInfo& InInstanceInfo) const
{
	if (Playlist)
	{
		return Playlist->GetPlaybackManager().FindPlaybackInstance(InInstanceInfo.InstanceId, InInstanceInfo.AssetPath, InInstanceInfo.ChannelName);
	}
	return nullptr;
}