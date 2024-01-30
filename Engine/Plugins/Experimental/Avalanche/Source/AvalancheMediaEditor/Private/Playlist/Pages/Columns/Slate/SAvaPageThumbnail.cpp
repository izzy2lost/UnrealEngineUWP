// Copyright Epic Games, Inc. All Rights Reserved.

#include "SAvaPageThumbnail.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "AssetThumbnail.h"
#include "Framework/Application/SlateApplication.h"
#include "Playlist/AvalanchePlaylist.h"
#include "Playlist/Pages/PageViews/AvaPageView.h"
#include "Playlist/Pages/Slate/SAvaPageViewRow.h"
#include "ThumbnailRendering/ThumbnailManager.h"
#include "Widgets/Layout/SScaleBox.h"

void SAvaPageThumbnail::Construct(const FArguments& InArgs, const FAvaPageViewPtr& InPageView, const TSharedPtr<SAvaPageViewRow>& InRow)
{
	PageViewWeak = InPageView;
	PageViewRowWeak = InRow;
	ThumbnailWidgetSize = InArgs._ThumbnailWidgetSize;

	InitThumbnailWidget();

	ChildSlot
	[
		SNew(SBorder)
		.Padding(FMargin(0.0f, 0.0f, 5.0f, 5.0f))
		.BorderImage(FAppStyle::Get().GetBrush("ContentBrowser.AssetTileItem.DropShadow"))
		[
			SNew(SBox)
			.Padding(0.0f)
			.WidthOverride(ThumbnailWidgetSize)
			.HeightOverride(ThumbnailWidgetSize)
			[
				SNew(SScaleBox)
				.Stretch(EStretch::ScaleToFit)
				.StretchDirection(EStretchDirection::Both)
				[
					SNew(SBorder)
					.Padding(0.0f)
					.BorderImage(FAppStyle::Get().GetBrush("ContentBrowser.AssetTileItem.ThumbnailAreaBackground"))
					[
						ThumbnailWidget.ToSharedRef()
					]
				]
			]
		]
	];
}

void SAvaPageThumbnail::OnAssetUpdate(const UAvalanchePlaylist* InPlaylist, const FAvalanchePage& InPage, EAvaPageChanges InPageChangeType)
{
	if (!EnumHasAnyFlags(InPageChangeType, EAvaPageChanges::Blueprint) || !InPlaylist)
	{
		return;
	}

	if (const TSharedPtr<IAvaPageView> PageView = PageViewWeak.Pin())
	{
		int32 PageIdToCheck = PageView->GetPageId();
		if (!PageView->IsTemplate())
		{
			PageIdToCheck = InPlaylist->GetPage(PageView->GetPageId()).GetTemplateId();
		}

		if (AssetThumbnail.IsValid() && InPage.IsValidPage() && InPage.IsTemplate() && PageIdToCheck == InPage.GetPageId())
		{
			const FSoftObjectPath AssetPath = InPage.GetAvalancheAssetPath(InPlaylist);
			const FAssetData Data = IAssetRegistry::Get()->GetAssetByObjectPath(AssetPath);

			AssetThumbnail->SetAsset(Data);
			AssetThumbnail->RefreshThumbnail();
			// Just to ensure that the viewport won't turn black on an asset which wasn't loaded yet
			bWasAssetLoaded = false;
		}
	}
}

void SAvaPageThumbnail::InitThumbnailWidget()
{
	if (const TSharedPtr<IAvaPageView> PageView =  PageViewWeak.Pin())
	{
		if (UAvalanchePlaylist* Playlist = PageView->GetPlaylist())
		{
			Playlist->GetOnPagesChanged().AddSP(this, &SAvaPageThumbnail::OnAssetUpdate);

			FAvalanchePage& Page = Playlist->GetPage(PageView->GetPageId());
			if (!PageView->IsTemplate())
			{
				Page = Playlist->GetPage(Page.GetTemplateId());
			}
			const FSoftObjectPath AssetPath = Page.GetAvalancheAssetPath(Playlist);
			const FAssetData Data = IAssetRegistry::Get()->GetAssetByObjectPath(AssetPath);


			AssetThumbnail = MakeShared<FAssetThumbnail>(Data, ThumbnailWidgetSize * 2, ThumbnailWidgetSize * 2, UThumbnailManager::Get().GetSharedThumbnailPool());
			AssetThumbnail->SetRealTime(false);

			ThumbnailWidget = AssetThumbnail->MakeThumbnailWidget();

			Tooltip = FSlateApplication::Get().MakeToolTip(NSLOCTEXT("SAvaPageThumbnail", "SAvaPageThumbnail_Tooltip", "Tooltip"));
			Tooltip->SetContentWidget(SNew(SBox)
					.Padding(0.0f)
					.WidthOverride(ThumbnailWidgetSize * 2)
					.HeightOverride(ThumbnailWidgetSize * 2)
					[
						SNew(SScaleBox)
						.Stretch(EStretch::ScaleToFit)
						.StretchDirection(EStretchDirection::Both)
						[
							AssetThumbnail->MakeThumbnailWidget()
						]
					]);

			ThumbnailWidget->SetToolTip(Tooltip);
		}
	}

	if (!ThumbnailWidget.IsValid())
	{
		ThumbnailWidget = SNullWidget::NullWidget;
	}
}

void SAvaPageThumbnail::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	if (!bWasAssetLoaded && AssetThumbnail->GetAsset())
	{
		bWasAssetLoaded = true;
		AssetThumbnail->RefreshThumbnail();
	}
}

SAvaPageThumbnail::~SAvaPageThumbnail()
{
	if (const TSharedPtr<IAvaPageView> PageView =  PageViewWeak.Pin())
	{
		if (UAvalanchePlaylist* Playlist = PageView->GetPlaylist())
		{
			Playlist->GetOnPagesChanged().RemoveAll(this);
		}
	}
}
