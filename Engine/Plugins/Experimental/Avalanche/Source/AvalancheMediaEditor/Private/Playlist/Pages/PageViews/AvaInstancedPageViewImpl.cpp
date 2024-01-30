// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaInstancedPageViewImpl.h"

#include "AvalancheBroadcast.h"
#include "Framework/Application/SlateApplication.h"
#include "Input/Reply.h"
#include "Playlist/AvalanchePage.h"
#include "Playlist/AvalanchePlaylist.h"
#include "Playlist/AvaPlaylistDefines.h"

#define LOCTEXT_NAMESPACE "AvaPageViewImpl"

FAvaInstancedPageViewImpl::FAvaInstancedPageViewImpl(int32 InPageId, UAvalanchePlaylist* InPlaylist, const TSharedPtr<SAvaPageList>& InPageList)
	: FAvaPageViewImpl(InPageId, InPlaylist, InPageList)
{
}

FReply FAvaInstancedPageViewImpl::OnPlayButtonClicked()
{
	UAvalanchePlaylist* Playlist = GetPlaylist();

	if (IsValid(Playlist))
	{
		const FAvalanchePage& Page = GetPage();

		if (Page.IsValidPage())
		{
			const TArray<FAvalanchePageStatus> Statuses = Page.GetPageProgramStatuses(Playlist);
			FModifierKeysState KeyState = FSlateApplication::Get().GetModifierKeys();

			const bool bContinue = KeyState.IsControlDown() || KeyState.IsCommandDown();

			if (bContinue)
			{
				Playlist->ContinuePage(Page.GetPageId(), false);
			}
			else if (FAvalanchePage::StatusesContainsStatus(Statuses, {EAvalanchePageStatus::Playing}))
			{
				Playlist->PlayPage(Page.GetPageId(), EAvaPlayType::PlayFromStart);
			}
			else
			{
				Playlist->PlayPage(Page.GetPageId(), EAvaPlayType::PlayFromStart);
			}

			return FReply::Handled();
		}
	}

	return FReply::Unhandled();
}

bool FAvaInstancedPageViewImpl::CanPlay() const
{
	UAvalanchePlaylist* Playlist = GetPlaylist();

	if (IsValid(Playlist))
	{
		const FAvalanchePage& Page = GetPage();

		if (Page.IsValidPage())
		{
			const FAvaOutputChannel& Channel = UAvalancheBroadcast::Get().GetCurrentProfile().GetChannel(Page.GetChannelName());

			if (Channel.GetMediaOutputs().Num() == 0)
			{
				return false;
			}

			const TArray<FAvalanchePageStatus> Statuses = Page.GetPageProgramStatuses(Playlist);

			const bool bHasError = FAvalanchePage::StatusesContainsStatus(Statuses, {EAvalanchePageStatus::Error, EAvalanchePageStatus::Loading,
				EAvalanchePageStatus::Missing, EAvalanchePageStatus::Syncing, EAvalanchePageStatus::Unknown});

			if (bHasError)
			{
				return false;
			}

			const bool bCanPlay = FAvalanchePage::StatusesContainsStatus(Statuses, {EAvalanchePageStatus::Available, EAvalanchePageStatus::Loaded,
				EAvalanchePageStatus::Playing, EAvalanchePageStatus::NeedsSync});

			if (bCanPlay)
			{
				return true;
			}
		}
	}

	return false;
}

ECheckBoxState FAvaInstancedPageViewImpl::IsEnabled() const
{
	const FAvalanchePage& Page = GetPage();
	if (Page.IsValidPage() && Page.IsEnabled())
	{
		return  ECheckBoxState::Checked;
	}
	return ECheckBoxState::Unchecked;
}

void FAvaInstancedPageViewImpl::SetEnabled(ECheckBoxState InState)
{
	if (!IsPageSelected())
	{
		SetPageSelection(EAvaPageViewSelectionChangeType::ReplaceSelection);
	}

	PerformWorkOnPages(LOCTEXT("SetEnabled", "Set Enabled"),
		[InState](FAvalanchePage& InPage)->bool
		{
			InPage.SetEnabled(InState == ECheckBoxState::Checked);
			return true;
		});
}

FName FAvaInstancedPageViewImpl::GetChannelName() const
{
	const FAvalanchePage& Page = GetPage();
	return Page.IsValidPage() ? Page.GetChannelName() : FName();
}

bool FAvaInstancedPageViewImpl::SetChannel(FName InChannel)
{
	if (!IsPageSelected())
	{
		SetPageSelection(EAvaPageViewSelectionChangeType::ReplaceSelection);
	}

	return PerformWorkOnPages(LOCTEXT("SetChannel", "Set Channel"),
		[this, InChannel](FAvalanchePage& InPage)->bool
		{
			InPage.SetChannelName(InChannel);
			GetPlaylist()->GetOnPagesChanged().Broadcast(GetPlaylist(), InPage, EAvaPageChanges::Channel);
			return true;
		});
}

const FAvalanchePage& FAvaInstancedPageViewImpl::GetTemplate() const
{
	UAvalanchePlaylist* Playlist = PlaylistWeak.Get();

	if (IsValid(Playlist))
	{
		const FAvalanchePage& Page = Playlist->GetPage(PageId);

		if (Page.IsValidPage())
		{
			if (Page.IsTemplate())
			{
				return Page;
			}

			const FAvalanchePage& Template = Playlist->GetPage(Page.GetTemplateId());

			if (Template.IsValidPage())
			{
				return Template;
			}
		}
	}

	return FAvalanchePage::NullPage;
}

FText FAvaInstancedPageViewImpl::GetTemplateDescription() const
{
	const FAvalanchePage& Template = GetTemplate();

	if (!Template.IsValidPage())
	{
		return FText::GetEmpty();
	}

	return FText::Format(
		LOCTEXT("TemplateFormat", "{0}: {1}"),
		FText::AsNumber(Template.GetPageId(), &UE::AvalanchePlaylist::FEditorMetrics::PageIdFormattingOptions),
		Template.GetPageDescription()
	);
}

bool FAvaInstancedPageViewImpl::IsTemplate() const 
{ 
	return false;
}

#undef LOCTEXT_NAMESPACE
