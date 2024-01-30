// Copyright Epic Games, Inc. All Rights Reserved.

#include "SAvaPageDetails.h"

#include "Async/Async.h"
#include "AvaMediaEditorStyle.h"
#include "IAvaMediaModule.h"
#include "Playlist/AvaPlaylistEditor.h"
#include "Playlist/AvalancheManagedInstanceCache.h"
#include "Playlist/AvalanchePage.h"
#include "Playlist/AvalanchePlaylist.h"
#include "Playlist/DetailsView/RemoteControl/Properties/SAvaPageRemoteControlProps.h"
#include "Playlist/Pages/Slate/SAvaInstancedPageList.h"
#include "Playlist/Pages/Slate/SAvaPageList.h"
#include "RemoteControl/Controllers/SAvaRCControllerPanel.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SAvaPageDetails"

void SAvaPageDetails::Construct(const FArguments& InArgs, const TSharedPtr<FAvaPlaylistEditor>& InPlaylistEditor)
{
	PlaylistEditorWeak = InPlaylistEditor;
	ActivePageId = FAvalanchePage::InvalidPageId;

	InPlaylistEditor->GetOnPageEvent().AddSP(this, &SAvaPageDetails::OnPageEvent);
	IAvaMediaModule::Get().GetManagedInstanceCache().OnEntryInvalidated.AddSP(this, &SAvaPageDetails::OnManagedInstanceCacheEntryInvalidated);

	TSharedRef<SHorizontalBox> AnimationHeader = SNew(SHorizontalBox);
	{
		static const TArray<FText> HeaderTitles
    	{
    		LOCTEXT("HeaderTitleAnimationName", "Animation Name"),
    		LOCTEXT("HeaderTitleLoopsToPlay", "Loops to Play"),
    		LOCTEXT("HeaderTitlePlaybackSpeed", "Playback Speed"),
    		LOCTEXT("HeaderTitlePlayMode", "Play Mode")
    	};

    	for (const FText& Title : HeaderTitles)
    	{
    		AnimationHeader->AddSlot()
    			.FillWidth(1.f)
    			[
    				SNew(STextBlock)
    				.Text(Title)
    				.Justification(ETextJustify::Center)
    			];
    	}
	}

	ChildSlot
	[
		SNew(SScrollBox)
		+ SScrollBox::Slot()
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(FMargin(10.f, 10.f, 10.f, 0.f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.VAlign(EVerticalAlignment::VAlign_Center)
				.MaxWidth(75.f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("PageId", "Page Id"))
					.MinDesiredWidth(75.f)
				]
				+ SHorizontalBox::Slot()
				.VAlign(EVerticalAlignment::VAlign_Center)
				.Padding(5.f, 0.f, 0.f, 0.f)
				.MaxWidth(70.f)
				[
					SNew(SEditableTextBox)
					.HintText(LOCTEXT("PageIdHint", "Page Id"))
					.OnTextCommitted(this, &SAvaPageDetails::OnPageIdCommitted)
					.Text(this, &SAvaPageDetails::GetPageId)
					.IsEnabled(this, &SAvaPageDetails::HasSelectedPage)
				]
				+ SHorizontalBox::Slot()
				.VAlign(EVerticalAlignment::VAlign_Center)
				.Padding(5.f, 0.f, 0.f, 0.f)
				.AutoWidth()
				[
					SNew(SButton)
					.ButtonStyle(FAppStyle::Get(), "SimpleButton")
					.ToolTipText(LOCTEXT("DuplicatePageTooltip", "DuplicatePage"))
					.OnClicked(this, &SAvaPageDetails::DuplicateSelectedPage)
					.IsEnabled(this, &SAvaPageDetails::HasSelectedPage)
					[
						SNew(SImage)
						.Image(FAppStyle::Get().GetBrush("GenericCommands.Duplicate"))
					]
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(FMargin(10.f, 3.f, 10.f, 0.f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.VAlign(EVerticalAlignment::VAlign_Center)
				.MaxWidth(75.f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("PageName", "Page Name"))
					.MinDesiredWidth(75.f)
				]
				+ SHorizontalBox::Slot()
				.VAlign(EVerticalAlignment::VAlign_Center)
				.Padding(5.f, 0.f, 0.f, 0.f)
				[
					SNew(SEditableTextBox)
					.HintText(LOCTEXT("PageNameHint", "Page Name"))
					.OnTextChanged(this, &SAvaPageDetails::OnPageNameChanged)
					.Text(this, &SAvaPageDetails::GetPageDescription)
					.IsEnabled(this, &SAvaPageDetails::HasSelectedPage)
				]
			]
			// Controllers
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(FMargin(0.f, 10.f, 0.f, 0.f))
			[
				SAssignNew(RCControllerPanel, SAvaRCControllerPanel, InPlaylistEditor)
			]
			// Exposed Properties
			+ SVerticalBox::Slot()
			.Padding(FMargin(0.f, 10.f, 0.f, 0.f))
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Left)
				.Padding(5.f, 0.f, 0.f, 0.f)
				.AutoWidth()
				[
					SNew(SButton)
					.ContentPadding(0)
					.ButtonStyle(FAppStyle::Get(), "NoBorder")
					.OnClicked(this, &SAvaPageDetails::ToggleExposedPropertiesVisibility)
					.ToolTipText(LOCTEXT("VisibilityButtonToolTip", "Toggle Exposed Properties Visibility"))
					.Content()
					[
						SNew(SImage)
						.Image(this, &SAvaPageDetails::GetExposedPropertiesVisibilityBrush)
					]
				]
				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Left)
				.Padding(5.f, 0.f, 0.f, 0.f)
				.AutoWidth()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("Properties", "Properties"))
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SAssignNew(RemoteControlProps, SAvaPageRemoteControlProps, InPlaylistEditor)
				.Visibility(EVisibility::Collapsed)
			]
		]
	];

	OnPageSelectionChanged({});
}

SAvaPageDetails::~SAvaPageDetails()
{
	if (const TSharedPtr<FAvaPlaylistEditor> PlaylistEditor = PlaylistEditorWeak.Pin())
	{
		PlaylistEditor->GetOnPageEvent().RemoveAll(this);
	}
	if (IAvaMediaModule::IsModuleLoaded())
	{
		IAvaMediaModule::Get().GetManagedInstanceCache().OnEntryInvalidated.RemoveAll(this);
	}
}

void SAvaPageDetails::OnPageEvent(const TArray<int32>& InSelectedPageIds, UE::AvalanchePlaylist::EPageEvent InPageEvent)
{
	if (InPageEvent == UE::AvalanchePlaylist::EPageEvent::SelectionChanged)
	{
		OnPageSelectionChanged(InSelectedPageIds);
		RemoteControlProps->Refresh(InSelectedPageIds);
		RCControllerPanel->Refresh(InSelectedPageIds);
	}
	else if (InPageEvent == UE::AvalanchePlaylist::EPageEvent::ReimportRequest)
	{
		// Note: all widgets are refreshed in this event handler to ensure it is done
		// after the cache is invalidated.
		OnPageSelectionChanged(InSelectedPageIds);
		// Only call SAvaPageRemoteControlProps::UpdateDefaultValuesAndRefresh to have it update the values of all selected pages.
		RemoteControlProps->UpdateDefaultValuesAndRefresh(InSelectedPageIds);
		// Pages values already updated above, only need to refresh UI.
		RCControllerPanel->Refresh(InSelectedPageIds);
	}
}

void SAvaPageDetails::OnPageSelectionChanged(const TArray<int32>& InSelectedPageIds)
{
	ActivePageId = InSelectedPageIds.IsEmpty() ? FAvalanchePage::InvalidPageId : InSelectedPageIds[0];
}

void SAvaPageDetails::OnManagedInstanceCacheEntryInvalidated(const FSoftObjectPath& InAssetPath)
{
	if (!bRefreshSelectedPageQueued)
	{
		if (TSharedPtr<FAvaPlaylistEditor> PlaylistEditor = PlaylistEditorWeak.Pin())
		{
			UAvalanchePlaylist* Playlist = PlaylistEditor->GetPlaylist();

			if (IsValid(Playlist))
			{
				const FAvalanchePage& SelectedPage = GetSelectedPage();

				if (SelectedPage.IsValidPage())
				{
					if (SelectedPage.GetAvalancheAssetPath(Playlist) == InAssetPath)
					{
						bRefreshSelectedPageQueued = true;
						// Queue a refresh on next tick.
						// We don't want to refresh immediately to avoid issues with
						// cascading events within the managed blueprint cache.
						TWeakPtr<SWidget> ThisWeak(AsShared());
						AsyncTask(ENamedThreads::GameThread, [ThisWeak]()
							{
								if (const TSharedPtr<SWidget> ThisWidget = ThisWeak.Pin())
								{
									SAvaPageDetails* AvaPageDetails = static_cast<SAvaPageDetails*>(ThisWidget.Get());
									AvaPageDetails->RefreshSelectedPage();
									AvaPageDetails->bRefreshSelectedPageQueued = false;
								}
							});
					}
				}
			}
		}
	}
}

FReply SAvaPageDetails::ToggleExposedPropertiesVisibility()
{
	if (RemoteControlProps->GetVisibility() == EVisibility::Collapsed)
	{
		RemoteControlProps->SetVisibility(EVisibility::SelfHitTestInvisible);
	}
	else
	{
		RemoteControlProps->SetVisibility(EVisibility::Collapsed);
	}

	return FReply::Handled();
}

const FSlateBrush* SAvaPageDetails::GetExposedPropertiesVisibilityBrush() const
{
	if (RemoteControlProps->GetVisibility() == EVisibility::Collapsed)
	{
		return FAppStyle::GetBrush("Level.NotVisibleHighlightIcon16x");
	}
	else
	{
		return FAppStyle::GetBrush("Level.VisibleHighlightIcon16x");
	}
}

const FAvalanchePage& SAvaPageDetails::GetSelectedPage() const
{
	const TSharedPtr<FAvaPlaylistEditor> PlaylistEditor = PlaylistEditorWeak.Pin();

	if (HasSelectedPage() && PlaylistEditor && PlaylistEditor->IsPlaylistValid())
	{
		return PlaylistEditor->GetPlaylist()->GetPage(ActivePageId);
	}

	return FAvalanchePage::NullPage;
}

FAvalanchePage& SAvaPageDetails::GetMutableSelectedPage() const
{
	const TSharedPtr<FAvaPlaylistEditor> PlaylistEditor = PlaylistEditorWeak.Pin();

	if (HasSelectedPage() && PlaylistEditor && PlaylistEditor->IsPlaylistValid())
	{
		return PlaylistEditor->GetPlaylist()->GetPage(ActivePageId);
	}

	return FAvalanchePage::NullPage;
}

void SAvaPageDetails::RefreshSelectedPage()
{
	const FAvalanchePage& SelectedPage = GetSelectedPage();

	if (SelectedPage.IsValidPage())
	{
		OnPageSelectionChanged({SelectedPage.GetPageId()});
		RemoteControlProps->UpdateDefaultValuesAndRefresh({SelectedPage.GetPageId()});
		RCControllerPanel->Refresh({SelectedPage.GetPageId()});
	}
}

bool SAvaPageDetails::HasSelectedPage() const
{
	if (ActivePageId == FAvalanchePage::InvalidPageId)
	{
		return false;
	}

	return PlaylistEditorWeak.IsValid();
}

FText SAvaPageDetails::GetPageId() const
{
	const FAvalanchePage& SelectedPage = GetSelectedPage();

	if (SelectedPage.IsValidPage())
	{
		return FText::AsNumber(SelectedPage.GetPageId(), &UE::AvalanchePlaylist::FEditorMetrics::PageIdFormattingOptions);
	}

	return FText::GetEmpty();
}

void SAvaPageDetails::OnPageIdCommitted(const FText& InNewText, ETextCommit::Type InCommitType)
{
	switch (InCommitType)
	{
		case ETextCommit::OnEnter:
		case ETextCommit::OnUserMovedFocus:
			break;

		case ETextCommit::Default:
		case ETextCommit::OnCleared:
		default:
			return;
	}

	if (!InNewText.IsNumeric())
	{
		return;
	}

	FAvalanchePage& SelectedPage = GetMutableSelectedPage();

	if (!SelectedPage.IsValidPage()) // Not FAvalanchePage::NullPage
	{
		return;
	}

	int32 NewId = FCString::Atoi(*InNewText.ToString());

	if (NewId == SelectedPage.GetPageId())
	{
		return;
	}

	const TSharedPtr<FAvaPlaylistEditor> PlaylistEditor = PlaylistEditorWeak.Pin();

	if (!PlaylistEditor.IsValid() || !IsValid(PlaylistEditor->GetPlaylist()))
	{
		return;
	}

	const bool bSuccessful = PlaylistEditor->GetPlaylist()->RenumberPageId(SelectedPage.GetPageId(), NewId);

	if (bSuccessful)
	{
		TSharedPtr<SAvaInstancedPageList> PageList = PlaylistEditor->GetActiveListWidget();
		
		if (PageList.IsValid())
		{
			PageList->SelectPage(NewId);
		}
	}
}

FText SAvaPageDetails::GetPageDescription() const
{
	const FAvalanchePage& SelectedPage = GetSelectedPage();

	if (SelectedPage.IsValidPage())
	{
		return SelectedPage.GetPageDescription();
	}

	return FText::GetEmpty();
}

void SAvaPageDetails::OnPageNameChanged(const FText& InNewText)
{
	FAvalanchePage& SelectedPage = GetMutableSelectedPage();

	if (SelectedPage.IsValidPage()) // Not FAvalanchePage::NullPage
	{
		SelectedPage.SetPageFriendlyName(InNewText);
	}
}

FReply SAvaPageDetails::DuplicateSelectedPage()
{
	FAvalanchePage& SelectedPage = GetMutableSelectedPage();

	if (!SelectedPage.IsValidPage()) // Not FAvalanchePage::NullPage
	{
		return FReply::Unhandled();
	}

	const TSharedPtr<FAvaPlaylistEditor> PlaylistEditor = PlaylistEditorWeak.Pin();

	if (!PlaylistEditor.IsValid())
	{
		return FReply::Unhandled();
	}

	TSharedPtr<SAvaInstancedPageList> PageList = PlaylistEditor->GetActiveListWidget();

	if (!PageList.IsValid())
	{
		return FReply::Unhandled();
	}

	TArray<int32> SelectedPages = TArray<int32>(PageList->GetSelectedPageIds());
	PageList->SelectPage(SelectedPage.GetPageId());
	PageList->DuplicateSelectedPages();
	PageList->SelectPages(SelectedPages);

	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
