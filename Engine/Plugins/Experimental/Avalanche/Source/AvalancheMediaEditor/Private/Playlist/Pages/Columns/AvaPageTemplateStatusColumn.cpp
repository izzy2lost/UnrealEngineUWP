// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaPageTemplateStatusColumn.h"

#include "AvalancheBroadcast.h"
#include "AvaMediaEditorStyle.h"
#include "Playlist/AvalanchePage.h"
#include "Playlist/AvalanchePlaylist.h"
#include "Playlist/Pages/PageViews/AvaPageView.h"
#include "Playlist/Pages/PageViews/AvaPageViewImpl.h"
#include "Playlist/Pages/PageViews/AvaTemplatePageViewImpl.h"
#include "Styling/StyleColors.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Views/SHeaderRow.h"

#define LOCTEXT_NAMESPACE "AvaPageStatusColumn"

FText FAvaPageTemplateStatusColumn::GetColumnDisplayNameText() const
{
	return LOCTEXT("PageId_Status", "Status");
}

FText FAvaPageTemplateStatusColumn::GetColumnToolTipText() const
{
	return LOCTEXT("PageId_ToolTip", "Page Status");
}

SHeaderRow::FColumn::FArguments FAvaPageTemplateStatusColumn::ConstructHeaderRowColumn()
{
	return SHeaderRow::Column(GetColumnId())
		.DefaultLabel(GetColumnDisplayNameText())
		.DefaultTooltip(GetColumnToolTipText())
		.FixedWidth(94.f)
		.ShouldGenerateWidget(true)
		.VAlignCell(EVerticalAlignment::VAlign_Center)
	;
}

TSharedRef<SWidget> FAvaPageTemplateStatusColumn::ConstructRowWidget(const FAvaPageViewRef& InPageView
	, const TSharedPtr<SAvaPageViewRow>& InRow)
{
	const TSharedRef<FAvaTemplatePageViewImpl> TemplatePageView = StaticCastSharedRef<FAvaTemplatePageViewImpl>(
		StaticCastSharedRef<FAvaPageViewImpl>(InPageView)
	);

	const TWeakPtr<FAvaTemplatePageViewImpl> TemplatePageViewWeak = TemplatePageView;
	
	TSharedRef<SHorizontalBox> ButtonList = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.Padding(UE::AvalanchePlaylist::FEditorMetrics::ColumnLeftOffset, 0.f, 0.f, 0.f)
		[
			SNew(SButton)
			.ButtonStyle(FAvaMediaEditorStyle::Get(), "AvalancheMediaEditor.BorderlessButton")
			.OnClicked(TemplatePageView, &FAvaTemplatePageViewImpl::OnPreviewButtonClicked)
			.IsEnabled(TemplatePageView, &FAvaTemplatePageViewImpl::CanPreview)
			.ToolTipText(LOCTEXT("Preview", "Preview\n\n- Click: Preview from start\n- +Shift: Use Preview Frame\n- +Control: Continue"))
			[
				SNew(SImage)
				.ColorAndOpacity_Static(&FAvaPageTemplateStatusColumn::GetIsPreviewingButtonColor, TemplatePageViewWeak)
				.Image(FAvaMediaEditorStyle::Get().GetBrush("AvalancheMediaEditor.MediaPreviewing"))
			]
		];

	ButtonList->AddSlot()
		.Padding(UE::AvalanchePlaylist::FEditorMetrics::ColumnLeftOffset, 0.f, 0.f, 0.f)
		[
			SNew(SButton)
			.ButtonStyle(FAvaMediaEditorStyle::Get(), "AvalancheMediaEditor.BorderlessButton")
			.OnClicked(TemplatePageView, &FAvaTemplatePageViewImpl::OnAssetStatusButtonClicked)
			.IsEnabled(TemplatePageView, &FAvaTemplatePageViewImpl::CanChangeAssetStatus)
			.ToolTipText_Static(&FAvaPageTemplateStatusColumn::GetAssetStatusButtonTooltip, TemplatePageViewWeak)
			[
				SNew(SImage)
				.ColorAndOpacity_Static(&FAvaPageTemplateStatusColumn::GetAssetStatusButtonColor, TemplatePageViewWeak)
				.Image(FAvaMediaEditorStyle::Get().GetBrush("AvalancheMediaEditor.MediaAssetStatus"))
			]
		];

	ButtonList->AddSlot()
		.Padding(UE::AvalanchePlaylist::FEditorMetrics::ColumnLeftOffset, 0.f, 0.f, 0.f)
		[
			SNew(SButton)
			.ButtonStyle(FAvaMediaEditorStyle::Get(), "AvalancheMediaEditor.BorderlessButton")
			.OnClicked(TemplatePageView, &FAvaTemplatePageViewImpl::OnSyncStatusButtonClicked)
			.IsEnabled(TemplatePageView, &FAvaTemplatePageViewImpl::CanChangeSyncStatus)
			.ToolTipText_Static(&FAvaPageTemplateStatusColumn::GetSyncStatusButtonTooltip, TemplatePageViewWeak)
			[
				SNew(SImage)
				.ColorAndOpacity_Static(&FAvaPageTemplateStatusColumn::GetSyncStatusButtonColor, TemplatePageViewWeak)
				.Image(FAvaMediaEditorStyle::Get().GetBrush("AvalancheMediaEditor.MediaSyncStatus"))
			]
		];

	return ButtonList;
}

FSlateColor FAvaPageTemplateStatusColumn::GetIsPreviewingButtonColor(const TWeakPtr<FAvaTemplatePageViewImpl> InPageViewWeak)
{
	static const FSlateColor Error(FStyleColors::AccentRed.GetSpecifiedColor());
	static const FSlateColor Syncing(FStyleColors::AccentOrange.GetSpecifiedColor());
	static const FSlateColor Loading(FStyleColors::AccentBlue.GetSpecifiedColor());
	static const FSlateColor NotPreviewing(FStyleColors::AccentGray.GetSpecifiedColor());
	static const FSlateColor Previewing(FStyleColors::AccentGreen.GetSpecifiedColor());

	if (const TSharedPtr<FAvaTemplatePageViewImpl> PageView = InPageViewWeak.Pin())
	{
		UAvalanchePlaylist* Previewlist = PageView->GetPlaylist();

		if (IsValid(Previewlist))
		{
			FAvalanchePage& Page = Previewlist->GetPage(PageView->GetPageId());

			if (Page.IsValidPage())
			{
				const TArray<FAvalanchePageStatus> Statuses = Page.GetPagePreviewStatuses(Previewlist);

				if (FAvalanchePage::StatusesContainsStatus(Statuses, {EAvalanchePageStatus::Unknown, 
					EAvalanchePageStatus::Error, 
					EAvalanchePageStatus::Missing}))
				{
					return Error;
				}

				if (FAvalanchePage::StatusesContainsStatus(Statuses, {EAvalanchePageStatus::Syncing}))
				{
					return Syncing;
				}

				if (FAvalanchePage::StatusesContainsStatus(Statuses, {EAvalanchePageStatus::Loading}))
				{
					return Loading;
				}

				if (FAvalanchePage::StatusesContainsStatus(Statuses, {EAvalanchePageStatus::Previewing}))
				{
					return Previewing;
				}

				if (FAvalanchePage::StatusesContainsStatus(Statuses, {EAvalanchePageStatus::Available,
					EAvalanchePageStatus::Loaded,
					EAvalanchePageStatus::Playing}))
				{
					return NotPreviewing;
				}
			}
		}
	}

	return Error;
}

FSlateColor FAvaPageTemplateStatusColumn::GetAssetStatusButtonColor(const TWeakPtr<FAvaTemplatePageViewImpl> InPageViewWeak)
{
	static const FSlateColor Error(FStyleColors::AccentRed.GetSpecifiedColor());
	static const FSlateColor Loading(FStyleColors::AccentBlue.GetSpecifiedColor());
	static const FSlateColor Available(FStyleColors::AccentYellow.GetSpecifiedColor());
	static const FSlateColor Loaded(FStyleColors::AccentGreen.GetSpecifiedColor());

	if (const TSharedPtr<FAvaTemplatePageViewImpl> PageView = InPageViewWeak.Pin())
	{
		UAvalanchePlaylist* Playlist = PageView->GetPlaylist();

		if (IsValid(Playlist))
		{
			FAvalanchePage& Page = Playlist->GetPage(PageView->GetPageId());

			if (Page.IsValidPage())
			{
				const TArray<FAvalanchePageStatus> Statuses = Page.GetPagePreviewStatuses(Playlist);

				if (FAvalanchePage::StatusesContainsStatus(Statuses, {EAvalanchePageStatus::Unknown, 
					EAvalanchePageStatus::Error, 
					EAvalanchePageStatus::Missing}))
				{
					return Error;
				}

				if (FAvalanchePage::StatusesContainsStatus(Statuses, {EAvalanchePageStatus::Loading}))
				{
					return Loading;
				}

				if (FAvalanchePage::StatusesContainsStatus(Statuses, {EAvalanchePageStatus::Loaded}))
				{
					return Loaded;
				}

				if (FAvalanchePage::StatusesContainsStatus(Statuses, {EAvalanchePageStatus::Available}))
				{
					return Available;
				}
			}
		}
	}

	return Error;
}

FText FAvaPageTemplateStatusColumn::GetAssetStatusButtonTooltip(const TWeakPtr<FAvaTemplatePageViewImpl> InPageViewWeak)
{
	if (const TSharedPtr<FAvaTemplatePageViewImpl> PageView = InPageViewWeak.Pin())
	{
		UAvalanchePlaylist* Playlist = PageView->GetPlaylist();

		if (IsValid(Playlist))
		{
			FAvalanchePage& Page = Playlist->GetPage(PageView->GetPageId());

			if (Page.IsValidPage())
			{
				UEnum* StatusEnum = StaticEnum<EAvalanchePageStatus>();
				const TArray<FAvalanchePageStatus> Statuses = Page.GetPagePreviewStatuses(Playlist);
				TArray<FText> StatusTexts;

				for (const FAvalanchePageStatus& Status : Statuses)
				{
					StatusTexts.Add(StatusEnum->GetDisplayNameTextByValue(static_cast<int64>(Status.Status)));
				}

				if (!StatusTexts.IsEmpty())
				{
					return FText::Format(
						LOCTEXT("StatusInfo", "Status: {0}."),
						FText::Join(LOCTEXT("Separator", ", "), StatusTexts)
					);
				}
			}
		}
	}

	return LOCTEXT("NoStatues", "Status Information Unavailable.");
}

FText FAvaPageTemplateStatusColumn::GetSyncStatusButtonTooltip(const TWeakPtr<FAvaTemplatePageViewImpl> InPageViewWeak)
{
	static FText Error = LOCTEXT("Error", "Storm Sync Error");
	static FText NeedsSync = LOCTEXT("NeedsSync", "Storm Sync Required");
	static FText Syncing = LOCTEXT("Syncing", "Storm Syncing");
	static FText Synced = LOCTEXT("Synced", "Storm Synced");

	if (const TSharedPtr<FAvaTemplatePageViewImpl> PageView = InPageViewWeak.Pin())
	{
		UAvalanchePlaylist* Playlist = PageView->GetPlaylist();

		if (IsValid(Playlist))
		{
			FAvalanchePage& Page = Playlist->GetPage(PageView->GetPageId());

			if (Page.IsValidPage())
			{
				const TArray<FAvalanchePageStatus> Statuses = Page.GetPagePreviewStatuses(Playlist);

				for (const FAvalanchePageStatus& Status : Statuses)
				{
					if (Status.bNeedsSync)
					{
						return NeedsSync;
					}
				}

				if (FAvalanchePage::StatusesContainsStatus(Statuses, {EAvalanchePageStatus::Syncing}))
				{
					return Syncing;
				}

				return Synced;
			}
		}
	}

	return Error;
}

FSlateColor FAvaPageTemplateStatusColumn::GetSyncStatusButtonColor(
	const TWeakPtr<FAvaTemplatePageViewImpl> InPageViewWeak)
{
	static const FSlateColor Error(FStyleColors::AccentRed.GetSpecifiedColor());
	static const FSlateColor NeedsSync(FStyleColors::AccentYellow.GetSpecifiedColor());
	static const FSlateColor Syncing(FStyleColors::AccentBlue.GetSpecifiedColor());
	static const FSlateColor Synced(FStyleColors::AccentGreen.GetSpecifiedColor());

	if (const TSharedPtr<FAvaTemplatePageViewImpl> PageView = InPageViewWeak.Pin())
	{
		UAvalanchePlaylist* Playlist = PageView->GetPlaylist();

		if (IsValid(Playlist))
		{
			FAvalanchePage& Page = Playlist->GetPage(PageView->GetPageId());

			if (Page.IsValidPage())
			{
				const TArray<FAvalanchePageStatus> Statuses = Page.GetPagePreviewStatuses(Playlist);

				for (const FAvalanchePageStatus& Status : Statuses)
				{
					if (Status.bNeedsSync)
					{
						return NeedsSync;
					}
				}

				if (FAvalanchePage::StatusesContainsStatus(Statuses, {EAvalanchePageStatus::Syncing}))
				{
					return Syncing;
				}

				return Synced;
			}
		}
	}

	return Error;
}

#undef LOCTEXT_NAMESPACE
