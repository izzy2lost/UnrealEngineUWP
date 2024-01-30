// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaPageThumbnailColumn.h"
#include "Slate/SAvaPageThumbnail.h"
#include "Widgets/Layout/SBox.h"

#define LOCTEXT_NAMESPACE "AvaPageThumbnailColumn"

FText FAvaPageThumbnailColumn::GetColumnDisplayNameText() const
{
	return LOCTEXT("PageThumbnail_Name", "Thumbnail");
}

FText FAvaPageThumbnailColumn::GetColumnToolTipText() const
{
	return LOCTEXT("PageThumbnail_Tooltip", "Preview thumbnail of the page, if available");
}

SHeaderRow::FColumn::FArguments FAvaPageThumbnailColumn::ConstructHeaderRowColumn()
{
	return SHeaderRow::Column(GetColumnId())
		.DefaultLabel(GetColumnDisplayNameText())
		.DefaultTooltip(GetColumnToolTipText())
		.FixedWidth(70.f)
		.ShouldGenerateWidget(true)
		.VAlignCell(EVerticalAlignment::VAlign_Center);
}

TSharedRef<SWidget> FAvaPageThumbnailColumn::ConstructRowWidget(const FAvaPageViewRef& InPageView
	, const TSharedPtr<SAvaPageViewRow>& InRow)
{
	return SNew(SBox)
		.Padding(UE::AvalanchePlaylist::FEditorMetrics::ColumnLeftOffset, 0.f, 0.f, 0.f)
		[
			SNew(SAvaPageThumbnail, InPageView, InRow)
			.ThumbnailWidgetSize(64)
		];
}

#undef LOCTEXT_NAMESPACE
