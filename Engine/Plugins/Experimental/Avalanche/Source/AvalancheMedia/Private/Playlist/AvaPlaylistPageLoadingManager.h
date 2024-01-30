// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Queue.h"
#include "Playlist/AvalanchePlaylist.h"

/**
 * Manager for page loading requests.
 *
 * The main problem this class is solving right now is the throttling of page loading.
 * We don't want to start loading all the pages at the same time because it leads to main thread
 * bottleneck both when the requests are made and when the levels are loaded. To alleviate that
 * we make the operations more granular by loading a subset of levels at any given time.
 */
class FAvaPlaylistPageLoadingManager final : public IAvaPlaylistPageLoadingManager
{
public:
	explicit FAvaPlaylistPageLoadingManager(UAvalanchePlaylist* InPlaylist);
	virtual ~FAvaPlaylistPageLoadingManager() override;

	//~ Begin IAvaPlaylistPageLoadingManager
	virtual bool RequestLoadPage(int32 InPageId, bool bInIsPreview, const FName& InPreviewChannelName) override;
	//~ End IAvaPlaylistPageLoadingManager

private:
	bool RequestLoadPageInternal(const FAvalanchePage& InPage, bool bInIsPreview, const FName& InPreviewChannelName);
	
	void HandlePlaybackInstanceStatusChanged(const FAvaMediaPlaybackInstance& InPlaybackInstance);
	void HandlePlaybackManagerBeginTick(float InDeltaSeconds);

	struct FInstanceInfo;
	TSharedPtr<FAvaMediaPlaybackInstance> FindInstance(const FInstanceInfo& InInstanceInfo) const;
	
private:
	UAvalanchePlaylist* Playlist = nullptr;
	TWeakPtr<FAvaMediaPlaybackManager> PlaybackManagerWeak;

	struct FPageLoadRequest
	{
		int32 PageId;
		bool bIsPreview;
		FName PreviewChannelName;

		bool operator==(FPageLoadRequest const& Other) const
		{
			return PageId == Other.PageId && bIsPreview == Other.bIsPreview && PreviewChannelName == Other.PreviewChannelName;
		}

		FORCEINLINE friend uint32 GetTypeHash(FPageLoadRequest const& This)
		{
			uint32 Hash = 0;
			Hash = HashCombine(Hash, GetTypeHash(This.PageId));
			Hash = HashCombine(Hash, GetTypeHash(This.PreviewChannelName));
			Hash = HashCombine(Hash, GetTypeHash(This.bIsPreview));
			return Hash;
		}
	};
	/** Ordered queue. Requests are executed in order. */
	TQueue<FPageLoadRequest> PendingRequestQueue;
	/** Unordered set to ensure uniqueness of requests. */
	TSet<FPageLoadRequest> PendingRequestSet;

	/** Instances currently loading. */
	struct FInstanceInfo
	{
		FGuid InstanceId;
		FSoftObjectPath AssetPath;
		FString ChannelName;
		EAvaMediaPlaybackStatus Status = EAvaMediaPlaybackStatus::Unknown;
		FDateTime SubmitTime;	// Time stamp when the load request was submitted.
	};
	TArray<FInstanceInfo> LoadingInstances;

	/** Maximum concurrent instance to load. */
	int32 MaxLoadingInstances = 1;
};
