// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaTemplatePageViewImpl.h"
#include "Playlist/AvalanchePlaylist.h"
#include "Input/Reply.h"
#include "Playlist/AvalanchePage.h"

FAvaTemplatePageViewImpl::FAvaTemplatePageViewImpl(int32 InPageId, UAvalanchePlaylist* InPlaylist, const TSharedPtr<SAvaPageList>& InPageList)
		: FAvaPageViewImpl(InPageId, InPlaylist, InPageList)
{
}

UAvalanchePlaylist* FAvaTemplatePageViewImpl::GetPlaylist() const
{ 
	return FAvaPageViewImpl::GetPlaylist(); 
}

FReply FAvaTemplatePageViewImpl::OnPlayButtonClicked()
{
	return FReply::Handled();
}

bool FAvaTemplatePageViewImpl::CanPlay() const
{
	return false;
}

FReply FAvaTemplatePageViewImpl::OnSyncStatusButtonClicked()
{
	if (!CanChangeSyncStatus())
	{
		return FReply::Unhandled();
	}

	UAvalanchePlaylist* Playlist = GetPlaylist();

	if (IsValid(Playlist))
	{
		const FAvalanchePage& Page = GetPage();

		if (Page.IsValidPage())
		{
			//TODO: Actual sync

			return FReply::Handled();
		}
	}

	return FReply::Unhandled();
}

bool FAvaTemplatePageViewImpl::CanChangeSyncStatus() const
{
	UAvalanchePlaylist* Playlist = GetPlaylist();

	if (IsValid(Playlist))
	{
		const FAvalanchePage& Page = GetPage();

		if (Page.IsValidPage())
		{
			TArray<FAvalanchePageStatus> Statuses = Page.GetPagePreviewStatuses(Playlist);
			bool bNeedsSync = false;

			for (const FAvalanchePageStatus& Status : Statuses)
			{
				if (Status.bNeedsSync)
				{
					bNeedsSync = true;
					break;
				}
			}

			return bNeedsSync;
		}
	}

	return false;
}

bool FAvaTemplatePageViewImpl::IsTemplate() const 
{ 
	return true;
}
