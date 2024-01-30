// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaPageViewImpl.h"

#include "AssetRegistry/AssetData.h"
#include "Framework/Application/SlateApplication.h"
#include "Playlist/AvaPlaylistDefines.h"
#include "Playlist/AvalanchePage.h"
#include "Playlist/AvalanchePlaylist.h"
#include "Playlist/Pages/Slate/SAvaPageList.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "AvaPageViewImpl"

FAvaPageViewImpl::FAvaPageViewImpl(int32 InPageId, UAvalanchePlaylist* InPlaylist, const TSharedPtr<SAvaPageList>& InPageList)
	: PageId(InPageId)
	, PlaylistWeak(InPlaylist)
	, PageListWeak(InPageList)
{
}

UAvalanchePlaylist* FAvaPageViewImpl::GetPlaylist() const
{
	return PlaylistWeak.Get();
}

int32 FAvaPageViewImpl::GetPageId() const
{
	const FAvalanchePage& Page = GetPage();
	return Page.IsValidPage()
		? Page.GetPageId()
		: FAvalanchePage::InvalidPageId;
}

FText FAvaPageViewImpl::GetPageIdText() const
{
	const int32 Id = GetPageId();
	return Id != FAvalanchePage::InvalidPageId
		? FText::AsNumber(Id, &UE::AvalanchePlaylist::FEditorMetrics::PageIdFormattingOptions)
		: LOCTEXT("InvalidIdText", "(invalid)");
}

FText FAvaPageViewImpl::GetPageNameText() const
{
	const FAvalanchePage& Page = GetPage();

	return Page.IsValidPage()
		? FText::FromString(Page.GetPageName())
		: LOCTEXT("EmptyPageNameText", "");
}

FText FAvaPageViewImpl::GetPageTransitionLayerNameText() const
{
	if (const UAvalanchePlaylist* Playlist = PlaylistWeak.Get())
	{
		const FAvalanchePage& Page = GetPage();
		if (Page.IsValidPage())
		{
			if (Page.HasTransitionLogic(Playlist))
			{
				FString TransitionLayers;
				const int32 NumTemplates = Page.GetNumTemplates(Playlist);
				for (int32 TemplateIndex = 0; TemplateIndex < NumTemplates; ++TemplateIndex)
				{
					if (!TransitionLayers.IsEmpty())
					{
						TransitionLayers += TEXT(", ");
					}
					TransitionLayers += Page.GetTransitionLayer(Playlist, TemplateIndex).ToString();
				}
				return FText::FromString(TransitionLayers);
			}

			return LOCTEXT("PageTransitionLayerText_NA", "N/A");
		}
	}
	// Either invalid playlist or invalid page.
	return LOCTEXT("PageTransitionLayerText_Invalid", "(invalid)");
}

FText FAvaPageViewImpl::GetPageSummary() const
{
	const FAvalanchePage& Page = GetPage();

	return Page.IsValidPage()
		? Page.GetPageSummary()
		: LOCTEXT("EmptyPageSummaryText", "");
}

FText FAvaPageViewImpl::GetPageDescription() const
{
	const FAvalanchePage& Page = GetPage();

	return Page.IsValidPage()
		? Page.GetPageDescription()
		: LOCTEXT("EmptyPageDescriptionText", "");
}

bool FAvaPageViewImpl::HasObjectPath(const UAvalanchePlaylist* InPlaylist) const
{
	const FAvalanchePage& Page = GetPage();
	return Page.IsValidPage() && !Page.IsComboTemplate();
}

FSoftObjectPath FAvaPageViewImpl::GetObjectPath(const UAvalanchePlaylist* InPlaylist) const
{
	const FAvalanchePage& Page = GetPage();
	if (Page.IsValidPage())
	{
		return Page.GetAvalancheAssetPath(InPlaylist);
	}

	return FSoftObjectPath();
}

FText FAvaPageViewImpl::GetObjectName(const UAvalanchePlaylist* InPlaylist) const
{
	const FAvalanchePage& Page = GetPage();
	if (Page.IsValidPage())
	{
		if (Page.ResolveTemplate(InPlaylist).IsComboTemplate())
		{
			// Since combo templates don't have an asset selector, use same placeholder.
			return LOCTEXT("AssetName_ComboPage_NA", "N/A");
		}
		return FText::FromString(Page.GetAvalancheAssetPath(InPlaylist).GetAssetName());
	}
	return FText::GetEmpty();
}

void FAvaPageViewImpl::OnObjectChanged(const FAssetData& InAssetData)
{
	if (!IsPageSelected())
	{
		SetPageSelection(EAvaPageViewSelectionChangeType::ReplaceSelection);
	}

	PerformWorkOnPages(LOCTEXT("SetAvaBlueprint", "Set Avalanche Blueprint"),
		[this, &InAssetData](FAvalanchePage& InPage)->bool
		{
			if (!InPage.UpdateAvalancheAsset(InAssetData.GetSoftObjectPath())) 
			{
 				return false;
			}
			GetPlaylist()->GetOnPagesChanged().Broadcast(GetPlaylist(), InPage, EAvaPageChanges::Blueprint);
			return true;
		});
}

bool FAvaPageViewImpl::Rename(const FText& InNewName)
{
	UAvalanchePlaylist* const Playlist = PlaylistWeak.Get();
	if (!Playlist)
	{
		return false;
	}

	FAvalanchePage& Page  = Playlist->GetPage(PageId);
	if (Page.IsValidPage())
	{
		FScopedTransaction Transaction(LOCTEXT("RenamePage", "Rename Page"));
		Playlist->Modify();
		
		Page.Rename(InNewName.ToString());
		return true;
	}
	return false;
}

bool FAvaPageViewImpl::RenameFriendlyName(const FText& InNewName)
{
	UAvalanchePlaylist* const Playlist = PlaylistWeak.Get();
	if (!Playlist)
	{
		return false;
	}

	FAvalanchePage& Page  = Playlist->GetPage(PageId);
	if (Page.IsValidPage())
	{
		FScopedTransaction Transaction(LOCTEXT("RenamePage", "Rename Page"));
		Playlist->Modify();
		
		Page.RenameFriendlyName(InNewName.ToString());
		return true;
	}
	return false;
}

FReply FAvaPageViewImpl::OnAssetStatusButtonClicked()
{
	return FReply::Handled();
}

bool FAvaPageViewImpl::CanChangeAssetStatus() const
{
	UAvalanchePlaylist* Playlist = PlaylistWeak.Get();

	if (IsValid(Playlist))
	{
		const FAvalanchePage& Page = Playlist->GetPage(PageId);

		if (Page.IsValidPage())
		{
			const TArray<FAvalanchePageStatus> Statuses = Page.GetPageContextualStatuses(Playlist);

			return !FAvalanchePage::StatusesContainsStatus(Statuses, {EAvalanchePageStatus::Error, EAvalanchePageStatus::Loaded, EAvalanchePageStatus::Missing,
				EAvalanchePageStatus::Playing, EAvalanchePageStatus::Previewing, EAvalanchePageStatus::Syncing, EAvalanchePageStatus::Unknown});
		}
	}

	return false;
}

FReply FAvaPageViewImpl::OnPreviewButtonClicked()
{
	UAvalanchePlaylist* Playlist = GetPlaylist();

	if (IsValid(Playlist))
	{
		const FAvalanchePage& Page = GetPage();

		if (Page.IsValidPage())
		{
			const TArray<FAvalanchePageStatus> Statuses = Page.GetPagePreviewStatuses(Playlist);
			const bool bIsPreviewing = FAvalanchePage::StatusesContainsStatus(Statuses, {EAvalanchePageStatus::Previewing});
			const int32 ThisPageId = Page.GetPageId();

			// Alt = restart
			// Control = continue
			// Shift = frame
			const bool bFromFrame = FSlateApplication::Get().GetModifierKeys().IsShiftDown();
			const bool bContinue = FSlateApplication::Get().GetModifierKeys().IsControlDown()
				|| FSlateApplication::Get().GetModifierKeys().IsCommandDown();

			const EAvaPlayType PreviewType = bFromFrame ? EAvaPlayType::PreviewFromFrame : EAvaPlayType::PreviewFromStart;

			if (bIsPreviewing && bContinue)
			{
				Playlist->ContinuePage(ThisPageId, true);
			}
			else
			{
				Playlist->PlayPage(ThisPageId, PreviewType);
			}

			return FReply::Handled();
		}
	}

	return FReply::Unhandled();
}

bool FAvaPageViewImpl::CanPreview() const
{
	UAvalanchePlaylist* Playlist = GetPlaylist();

	if (IsValid(Playlist))
	{
		const FAvalanchePage& Page = GetPage();

		if (Page.IsValidPage())
		{
			const TArray<FAvalanchePageStatus> Statuses = Page.GetPagePreviewStatuses(Playlist);
			const bool bIsPreviewing = FAvalanchePage::StatusesContainsStatus(Statuses, {EAvalanchePageStatus::Previewing});
			const int32 ThisPageId = Page.GetPageId();

			// Alt = restart
			// Control = continue
			// Shift = frame
			const bool bFromFrame = FSlateApplication::Get().GetModifierKeys().IsShiftDown();
			const bool bContinue = FSlateApplication::Get().GetModifierKeys().IsControlDown()
				|| FSlateApplication::Get().GetModifierKeys().IsCommandDown();
			const bool bRestartPreview = FSlateApplication::Get().GetModifierKeys().IsAltDown();

			if (bIsPreviewing)
			{
				if (bContinue)
				{
					return Playlist->CanContinuePage(ThisPageId, true);
				}
				else
				{
					return Playlist->CanStopPage(ThisPageId, EAvaPlaylistPageStopOptions::Default, true);

					// Unable to test if we can play as well.
				}
			}
			else
			{
				return Playlist->CanPlayPage(ThisPageId, true);
			}
		}
	}

	return false;
}

bool FAvaPageViewImpl::IsPageSelected() const
{
	TSharedPtr<SAvaPageList> PageList = PageListWeak.Pin();
	if (!PageList.IsValid())
	{
		return false;
	}

	return PageList->GetSelectedPageIds().Contains(PageId);
}

bool FAvaPageViewImpl::SetPageSelection(EAvaPageViewSelectionChangeType InSelectionChangeType)
{
	TSharedPtr<SAvaPageList> PageList = PageListWeak.Pin();
	if (!PageList.IsValid())
	{
		return false;
	}

	switch (InSelectionChangeType)
	{
		case EAvaPageViewSelectionChangeType::Deselect:
			if (PageList->GetSelectedPageIds().Contains(PageId))
			{
				PageList->DeselectPage(PageId);
			}

			return true;

		case EAvaPageViewSelectionChangeType::AddToSelection:
			if (!PageList->GetSelectedPageIds().Contains(PageId))
			{
				PageList->SelectPage(PageId, false);
			}

			return true;

		case EAvaPageViewSelectionChangeType::ReplaceSelection:
			PageList->DeselectPages();
			PageList->SelectPage(PageId, false);
			return true;

		default:
			// Not possible
			return false;
	}
}

bool FAvaPageViewImpl::PerformWorkOnPages(const FText& InTransactionSessionName, TFunction<bool(FAvalanchePage&)>&& InWork)
{
	UAvalanchePlaylist* const Playlist = PlaylistWeak.Get();
	if (!Playlist)
	{
		return false;
	}

	FAvalanchePage& UnderlyingPage  = Playlist->GetPage(PageId);
	if (!UnderlyingPage.IsValidPage())
	{
		return false;
	}

	TSet<FAvalanchePage*> PagesToPerformWork;
	PagesToPerformWork.Add(&UnderlyingPage);

	if (TSharedPtr<SAvaPageList> PageList = PageListWeak.Pin())
	{
		TConstArrayView<int32> SelectedPageIds = PageList->GetSelectedPageIds();

		if (SelectedPageIds.Contains(UnderlyingPage.GetPageId()))
		{
			for (int32 SelectedPageId : SelectedPageIds)
			{
				FAvalanchePage& CurrentPage = Playlist->GetPage(SelectedPageId);

				if (CurrentPage.IsValidPage())
				{
					PagesToPerformWork.Add(&CurrentPage);
				}
			}
		}
	}
	
	if (!PagesToPerformWork.IsEmpty())
	{
		FScopedTransaction Transaction(InTransactionSessionName);
		Playlist->Modify();

		int32 WorkDoneCount = 0;
		
		for (FAvalanchePage* const PageToPerformWork : PagesToPerformWork)
		{
			if (PageToPerformWork && InWork(*PageToPerformWork))
			{
				++WorkDoneCount;
			}
		}

		if (WorkDoneCount == 0)
		{
			Transaction.Cancel();
		}
		return WorkDoneCount > 0;
	}

	return false;
}

const FAvalanchePage& FAvaPageViewImpl::GetPage() const
{
	if (PlaylistWeak.IsValid())
	{
		return PlaylistWeak->GetPage(PageId);
	}
	return FAvalanchePage::NullPage;
}

#undef LOCTEXT_NAMESPACE
