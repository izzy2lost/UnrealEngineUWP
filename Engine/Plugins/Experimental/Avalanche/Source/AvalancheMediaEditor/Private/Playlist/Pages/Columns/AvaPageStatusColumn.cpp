// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaPageStatusColumn.h"

#include "AvalancheBroadcast.h"
#include "AvaMediaEditorStyle.h"
#include "Playlist/AvalanchePage.h"
#include "Playlist/AvalanchePlaylist.h"
#include "Playlist/Pages/PageViews/AvaInstancedPageViewImpl.h"
#include "Playlist/Pages/PageViews/AvaPageView.h"
#include "Styling/StyleColors.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Views/SHeaderRow.h"

#define LOCTEXT_NAMESPACE "AvaPageStatusColumn"

FText FAvaPageStatusColumn::GetColumnDisplayNameText() const
{
	return LOCTEXT("PageId_Status", "Status");
}

FText FAvaPageStatusColumn::GetColumnToolTipText() const
{
	return LOCTEXT("PageId_ToolTip", "Page Status");
}

SHeaderRow::FColumn::FArguments FAvaPageStatusColumn::ConstructHeaderRowColumn()
{
	return SHeaderRow::Column(GetColumnId())
		.DefaultLabel(GetColumnDisplayNameText())
		.DefaultTooltip(GetColumnToolTipText())
		.FixedWidth(94.f)
		.ShouldGenerateWidget(true)
		.VAlignCell(EVerticalAlignment::VAlign_Center)
	;
}

TSharedRef<SWidget> FAvaPageStatusColumn::ConstructRowWidget(const FAvaPageViewRef& InPageView
	, const TSharedPtr<SAvaPageViewRow>& InRow)
{
	const TSharedRef<FAvaInstancedPageViewImpl> InstancedPageView = StaticCastSharedRef<FAvaInstancedPageViewImpl>(
		StaticCastSharedRef<FAvaPageViewImpl>(InPageView)
		);

	const TWeakPtr<FAvaInstancedPageViewImpl> InstancedPageViewWeak = InstancedPageView;
	
	TSharedRef<SHorizontalBox> ButtonList = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.Padding(UE::AvalanchePlaylist::FEditorMetrics::ColumnLeftOffset, 0.f, 0.f, 0.f)
		[
			SNew(SButton)
			.ButtonStyle(FAvaMediaEditorStyle::Get(), "AvalancheMediaEditor.BorderlessButton")
			.OnClicked(InPageView, &IAvaPageView::OnPreviewButtonClicked)
			.IsEnabled(InPageView, &IAvaPageView::CanPreview)
			.ToolTipText(LOCTEXT("Preview", "Preview\n\n- Click: Preview from start\n- +Shift: Use Preview Frame\n- +Control: Continue"))
			[
				SNew(SBorder)
				.Padding(FMargin(0.f, 0.f))
				.BorderImage_Static(&FAvaPageStatusColumn::GetPreviewBorderBackgroundImage, InstancedPageViewWeak)
				.BorderBackgroundColor(FLinearColor(1.0f, 0.0f, 0.0f, 1.0f))
				[
					SNew(SImage)
					.ColorAndOpacity_Static(&FAvaPageStatusColumn::GetIsPreviewingButtonColor, InstancedPageViewWeak)
					.Image(FAvaMediaEditorStyle::Get().GetBrush("AvalancheMediaEditor.MediaPreviewing"))
				]
			]
		];
	
	ButtonList->AddSlot()
		.Padding(UE::AvalanchePlaylist::FEditorMetrics::ColumnLeftOffset, 0.f, 0.f, 0.f)
		[
			SNew(SButton)
			.ButtonStyle(FAvaMediaEditorStyle::Get(), "AvalancheMediaEditor.BorderlessButton")
			.OnClicked(InstancedPageView, &FAvaInstancedPageViewImpl::OnAssetStatusButtonClicked)
			.IsEnabled(InstancedPageView, &FAvaInstancedPageViewImpl::CanChangeAssetStatus)
			.ToolTipText_Static(&FAvaPageStatusColumn::GetAssetStatusButtonTooltip, InstancedPageViewWeak)
			[
				SNew(SImage)
				.ColorAndOpacity_Static(&FAvaPageStatusColumn::GetAssetStatusButtonColor, InstancedPageViewWeak)
				.Image(FAvaMediaEditorStyle::Get().GetBrush("AvalancheMediaEditor.MediaAssetStatus"))
			]
		];

	ButtonList->AddSlot()
		.Padding(UE::AvalanchePlaylist::FEditorMetrics::ColumnLeftOffset, 0.f, 0.f, 0.f)
		[
			SNew(SBorder)
			.Padding(FMargin(5.f, 4.f))
			.BorderImage_Static(&FAvaPageStatusColumn::GetProgramBorderBackgroundImage, InstancedPageViewWeak)
			.BorderBackgroundColor(FLinearColor(1.0f, 0.0f, 0.0f, 1.0f))
			.ToolTipText_Static(&FAvaPageStatusColumn::GetTakeInTooltip, InstancedPageViewWeak)
			[
				SNew(SImage)
				.ColorAndOpacity_Static(&FAvaPageStatusColumn::GetIsPlayingButtonColor, InstancedPageViewWeak)
				.Image(FAvaMediaEditorStyle::Get().GetBrush("AvalancheMediaEditor.MediaPlaying"))
			]
		];

	return ButtonList;
}

FSlateColor FAvaPageStatusColumn::GetIsPreviewingButtonColor(const TWeakPtr<FAvaInstancedPageViewImpl> InPageViewWeak)
{
	static const FSlateColor Error(FStyleColors::AccentRed.GetSpecifiedColor());
	static const FSlateColor Syncing(FStyleColors::AccentOrange.GetSpecifiedColor());
	static const FSlateColor Loading(FStyleColors::AccentBlue.GetSpecifiedColor());
	static const FSlateColor NotPreviewing(FStyleColors::AccentGray.GetSpecifiedColor());
	static const FSlateColor Previewing(FStyleColors::AccentGreen.GetSpecifiedColor());
	
	if (const FAvaPageViewPtr PageView = InPageViewWeak.Pin())
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

const FSlateBrush* FAvaPageStatusColumn::GetPreviewBorderBackgroundImage(const TWeakPtr<FAvaInstancedPageViewImpl> InPageViewWeak)
{
	bool bIsPlayHead = false;
	
	if (const FAvaPageViewPtr PageView = InPageViewWeak.Pin())
	{
		const UAvalanchePlaylist* Playlist = PageView->GetPlaylist();

		if (IsValid(Playlist))
		{
			if (Playlist->IsPagePreviewing(PageView->GetPageId()))
			{
				if (const FAvaPageListPlaybackContextCollection* ContextCollection = Playlist->GetPageListPlaybackContextCollection())
				{
					if (const TSharedPtr<FAvaPageListPlaybackContext> Context = ContextCollection->GetContext(true, Playlist->GetDefaultPreviewChannelName()))
					{
						bIsPlayHead = Context->PlayHeadPageId == PageView->GetPageId();
					}
				}
			}
		}
	}
	
	if (bIsPlayHead)
	{
		return FAppStyle::GetBrush("Border");
	}
	return FAppStyle::GetBrush("NoBorder");
}

FSlateColor FAvaPageStatusColumn::GetIsPlayingButtonColor(const TWeakPtr<FAvaInstancedPageViewImpl> InPageViewWeak)
{
	static const FSlateColor Error(FStyleColors::AccentRed.GetSpecifiedColor());
	static const FSlateColor NotPlaying(FStyleColors::AccentGray.GetSpecifiedColor());
	static const FSlateColor Playing(FStyleColors::AccentGreen.GetSpecifiedColor());

	if (const FAvaPageViewPtr PageView = InPageViewWeak.Pin())
	{
		UAvalanchePlaylist* Playlist = PageView->GetPlaylist();

		if (IsValid(Playlist))
		{
			FAvalanchePage& Page = Playlist->GetPage(PageView->GetPageId());

			if (Page.IsValidPage())
			{
				const TArray<FAvalanchePageStatus> Statuses = Page.GetPageProgramStatuses(Playlist);

				if (FAvalanchePage::StatusesContainsStatus(Statuses, {EAvalanchePageStatus::Playing}))
				{
					return Playing;
				}

				if (FAvalanchePage::StatusesContainsStatus(Statuses, {EAvalanchePageStatus::Unknown,
					EAvalanchePageStatus::Offline,
					EAvalanchePageStatus::Missing,
					EAvalanchePageStatus::Error}))
				{
					return Error;
				}
			}
		}
	}

	return NotPlaying;
}

const FSlateBrush* FAvaPageStatusColumn::GetProgramBorderBackgroundImage(const TWeakPtr<FAvaInstancedPageViewImpl> InPageViewWeak)
{
	bool bIsPlayHead = false;
	
	if (const FAvaPageViewPtr PageView = InPageViewWeak.Pin())
	{
		const UAvalanchePlaylist* Playlist = PageView->GetPlaylist();

		if (IsValid(Playlist))
		{
			if (Playlist->IsPagePlaying(PageView->GetPageId()))
			{
				if (const FAvaPageListPlaybackContextCollection* ContextCollection = Playlist->GetPageListPlaybackContextCollection())
				{
					if (const TSharedPtr<FAvaPageListPlaybackContext> Context = ContextCollection->GetContext(false, NAME_None))
					{
						bIsPlayHead = Context->PlayHeadPageId == PageView->GetPageId();
					}
				}
			}
		}
	}
	
	if (bIsPlayHead)
	{
		return FAppStyle::GetBrush("Border");
	}
	return FAppStyle::GetBrush("NoBorder");
}

FText FAvaPageStatusColumn::GetTakeInTooltip(const TWeakPtr<FAvaInstancedPageViewImpl> InPageViewWeak)
{
	// static const FText BaseTooltip = LOCTEXT("TakeIn", "Take In\n\n- Click: Play\n- Control+Click: Continue");
	static const FText BaseTooltip = LOCTEXT("Status", "Page Status: ");
	static const FText PlayedTooltip = LOCTEXT("Playing", "Playing");
	static const FText NotPlayedTooltip = LOCTEXT("Stopped", "Stopped");
	static const FText ErrorTooltip = LOCTEXT("CantPlay", "**Cannot Take In**");
	static const FText NoOutputs = LOCTEXT("NoOutputs", "No Outputs Selected");

	if (const FAvaPageViewPtr PageView = InPageViewWeak.Pin())
	{
		UAvalanchePlaylist* Playlist = PageView->GetPlaylist();

		if (IsValid(Playlist))
		{
			TArray<FText> Texts;
			Texts.Add(BaseTooltip);
			bool bAddedErrorTooltip = false;

			// Checks whether it can play based on situation, not status
			if (!Playlist->CanPlayPage(PageView->GetPageId(), false))
			{
				Texts.Add(ErrorTooltip);
				bAddedErrorTooltip = true;
			}

			const FAvalanchePage& Page = Playlist->GetPage(PageView->GetPageId());

			// Check actual status
			if (Page.IsValidPage())
			{
				const FAvaOutputChannel& Channel = UAvalancheBroadcast::Get().GetCurrentProfile().GetChannel(Page.GetChannelName());

				if (Channel.GetMediaOutputs().IsEmpty())
				{
					if (!bAddedErrorTooltip)
					{
						Texts.Add(ErrorTooltip);
						bAddedErrorTooltip = true;
					}

					Texts.Add(NoOutputs);
				}
				else
				{
					if (Playlist->IsPagePlaying(Page))
					{
						Texts.Add(PlayedTooltip);
					}
					else
					{
						Texts.Add(NotPlayedTooltip);
					}
				}
			}

			return FText::Join(LOCTEXT("NewLines", "\n\n"), Texts);
		}
	}

	return BaseTooltip;
}

FSlateColor FAvaPageStatusColumn::GetAssetStatusButtonColor(const TWeakPtr<FAvaInstancedPageViewImpl> InPageViewWeak)
{
	static const FSlateColor Error(FStyleColors::AccentRed.GetSpecifiedColor());
	static const FSlateColor Loading(FStyleColors::AccentBlue.GetSpecifiedColor());
	static const FSlateColor Available(FStyleColors::AccentYellow.GetSpecifiedColor());
	static const FSlateColor Loaded(FStyleColors::AccentGreen.GetSpecifiedColor());
	static const FSlateColor Playing(FStyleColors::AccentOrange.GetSpecifiedColor());

	if (const TSharedPtr<FAvaInstancedPageViewImpl> PageView = InPageViewWeak.Pin())
	{
		UAvalanchePlaylist* Playlist = PageView->GetPlaylist();

		if (IsValid(Playlist))
		{
			FAvalanchePage& Page = Playlist->GetPage(PageView->GetPageId());

			if (Page.IsValidPage())
			{
				const TArray<FAvalanchePageStatus> Statuses = Page.GetPageProgramStatuses(Playlist);

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

				if (FAvalanchePage::StatusesContainsStatus(Statuses, {EAvalanchePageStatus::Previewing,
					EAvalanchePageStatus::Playing}))
				{
					return Playing;
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

FText FAvaPageStatusColumn::GetAssetStatusButtonTooltip(const TWeakPtr<FAvaInstancedPageViewImpl> InPageViewWeak)
{
	if (const TSharedPtr<FAvaInstancedPageViewImpl> PageView = InPageViewWeak.Pin())
	{
		UAvalanchePlaylist* Playlist = PageView->GetPlaylist();

		if (IsValid(Playlist))
		{
			FAvalanchePage& Page = Playlist->GetPage(PageView->GetPageId());

			if (Page.IsValidPage())
			{
				const UEnum* PageStatusEnum = StaticEnum<EAvalanchePageStatus>();
				const UEnum* ChannelTypeEnum = StaticEnum<EAvaBroadcastChannelType>();
				const TArray<FAvalanchePageStatus> Statuses = Page.GetPageStatuses(Playlist);
				TArray<FText> StatusTexts;
				StatusTexts.Reserve(Statuses.Num());

				for (const FAvalanchePageStatus& Status : Statuses)
				{
					FText StatusTypeText = FText::Format(LOCTEXT("StatusType", "{0}: {1}"),
						ChannelTypeEnum->GetDisplayNameTextByValue(static_cast<int64>(Status.Type)),
						PageStatusEnum->GetDisplayNameTextByValue(static_cast<int64>(Status.Status)));
					
					StatusTexts.Add(MoveTemp(StatusTypeText));
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

#undef LOCTEXT_NAMESPACE
