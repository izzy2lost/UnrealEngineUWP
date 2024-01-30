// Copyright Epic Games, Inc. All Rights Reserved.

#include "Playlist/AvaPlaylistPlaybackUtils.h"

#include "Playlist/AvalanchePlaylist.h"

namespace UE::AvaPlaylistPlaybackUtils::Private
{
	template <typename Predicate>
	TArray<int32> FilterSelectedOrPreviewingPages(const UAvalanchePlaylist* InPlaylist, const TArray<int32>& InSelectedPageIds, FName InPreviewChannelName, Predicate InPredicate)
	{
		if (InPlaylist)
		{
			if (InSelectedPageIds.Num())
			{
				return ::Invoke(InPredicate, InPlaylist, InSelectedPageIds);
			}
			return ::Invoke(InPredicate, InPlaylist, InPlaylist->GetPreviewingPageIds(InPreviewChannelName));
		}
		return {};
	}
}

// Selecting the "next page" implies we know what the last page played was and we go to the next.
// Normally, the last page played was the one currently selected, but the user might decide to select
// another page for editing purpose, but still want to hit play next from the last played page.
// This is why the "play head" is now added in the playback context for the given list.
int32 FAvaPlaylistPlaybackUtils::GetPageIdToPlayNext(const UAvalanchePlaylist* InPlaylist, const FAvaPageListReference& InPageListReference, bool bInPreview, FName InPreviewChannelName)
{
	// If there is no playback context, it means no pages are playing and can't have a next page if nothing is playing.
	if (InPlaylist && InPlaylist->GetPageListPlaybackContextCollection())
	{
		if (const TSharedPtr<FAvaPageListPlaybackContext> PlaybackContext = InPlaylist->GetPageListPlaybackContextCollection()->GetContext(bInPreview, InPreviewChannelName))
		{
			// Check if the current play head page is playing.
			const bool bIsHeadPlaying = bInPreview ? InPlaylist->IsPagePreviewing(PlaybackContext->PlayHeadPageId) : InPlaylist->IsPagePlaying(PlaybackContext->PlayHeadPageId); 
			
			if (bIsHeadPlaying)
			{
				const int32 NextPageId = InPlaylist->GetNextPage(PlaybackContext->PlayHeadPageId, InPageListReference).GetPageId();
				if (IsPageIdValid(NextPageId) && InPlaylist->CanPlayPage(NextPageId, true))
				{
					return NextPageId;
				}
			}
		}
	}
	return FAvalanchePage::InvalidPageId;
}

TArray<int32> FAvaPlaylistPlaybackUtils::GetPagesToTakeToProgram(const UAvalanchePlaylist* InPlaylist, const TArray<int32>& InSelectedPageIds, FName InPreviewChannel)
{
	using namespace UE::AvaPlaylistPlaybackUtils::Private;
	auto KeepPagesToTakeToProgram = [](const UAvalanchePlaylist* InPlaylist, const TArray<int32>& InPageIds)
	{
		TArray<int32> OutPageIds;
		OutPageIds.Reserve(InPageIds.Num());
		for (int32 PageId : InPageIds)
		{
			// We can't take templates to program. Only instanced pages.
			// Note: CanPlayPage returns true for playing pages, so need to make sure it is not already playing.
			if (InPlaylist->CanPlayPage(PageId, false) && !InPlaylist->IsPagePlaying(PageId))
			{
				OutPageIds.Add(PageId);
			}
		}
		return OutPageIds;
	};
	return FilterSelectedOrPreviewingPages(InPlaylist, InSelectedPageIds, InPreviewChannel, KeepPagesToTakeToProgram);
}
