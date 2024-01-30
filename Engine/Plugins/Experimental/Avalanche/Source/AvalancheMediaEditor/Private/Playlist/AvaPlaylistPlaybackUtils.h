// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Playlist/AvalanchePage.h"

class UAvalanchePlaylist;

struct FAvaPlaylistPlaybackUtils
{
	static bool IsPageIdValid(int32 InPageId)
	{
		return InPageId != FAvalanchePage::InvalidPageId;
	}
	
	static int32 GetPageIdToPlayNext(const UAvalanchePlaylist* InPlaylist, const FAvaPageListReference& InPageListReference, bool bInPreview, FName InPreviewChannelName);
	
	static TArray<int32> GetPagesToTakeToProgram(const UAvalanchePlaylist* InPlaylist, const TArray<int32>& InSelectedPageIds, FName InPreviewChannel = NAME_None);
};