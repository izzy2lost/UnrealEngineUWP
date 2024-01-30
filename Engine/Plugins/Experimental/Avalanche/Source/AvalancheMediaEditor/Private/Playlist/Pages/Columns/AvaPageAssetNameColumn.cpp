// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaPageAssetNameColumn.h"
#include "Playlist/Pages/PageViews/AvaInstancedPageViewImpl.h"
#include "Playlist/Pages/PageViews/AvaTemplatePageViewImpl.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "AvaPageAssetNameColumn"

FText FAvaPageAssetNameColumn::GetColumnDisplayNameText() const
{
	return LOCTEXT("PageId_AssetName", "Asset");
}

FText FAvaPageAssetNameColumn::GetColumnToolTipText() const
{
	return LOCTEXT("PageId_ToolTip", "Name and Id of the template used to create this page.");
}

SHeaderRow::FColumn::FArguments FAvaPageAssetNameColumn::ConstructHeaderRowColumn()
{
	return SHeaderRow::Column(GetColumnId())
		.DefaultLabel(GetColumnDisplayNameText())
		.DefaultTooltip(GetColumnToolTipText())
		.FillWidth(0.1f)
		.ShouldGenerateWidget(true)
		.VAlignCell(EVerticalAlignment::VAlign_Center)
		;
}

TSharedRef<SWidget> FAvaPageAssetNameColumn::ConstructRowWidget(const FAvaPageViewRef& InPageView
	, const TSharedPtr<SAvaPageViewRow>& InRow)
{
	const TSharedRef<FAvaPageViewImpl> InstancedPageView = StaticCastSharedRef<FAvaPageViewImpl>(InPageView);
	const UAvalanchePlaylist* Playlist = InPageView->GetPlaylist();

	return SNew(SBox)
		.Padding(UE::AvalanchePlaylist::FEditorMetrics::ColumnLeftOffset, 0.f, 0.f, 0.f)
		[
			SNew(STextBlock)
			.Text(InstancedPageView, &FAvaPageViewImpl::GetObjectName, Playlist)
		];
}

#undef LOCTEXT_NAMESPACE
