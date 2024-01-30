// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaPageIdColumn.h"
#include "Playlist/Pages/PageViews/AvaInstancedPageView.h"
#include "Slate/SAvaPageId.h"
#include "Widgets/Layout/SBox.h"

#define LOCTEXT_NAMESPACE "AvaPageIdColumn"

FText FAvaPageIdColumn::GetColumnDisplayNameText() const
{
	return LOCTEXT("PageId_Name", "Page #");
}

FText FAvaPageIdColumn::GetColumnToolTipText() const
{
	return LOCTEXT("PageId_ToolTip", "Page Id");
}

SHeaderRow::FColumn::FArguments FAvaPageIdColumn::ConstructHeaderRowColumn()
{
	return SHeaderRow::Column(GetColumnId())
		.DefaultLabel(GetColumnDisplayNameText())
		.DefaultTooltip(GetColumnToolTipText())
		.FixedWidth(50.f)
		.ShouldGenerateWidget(true)
		.VAlignCell(EVerticalAlignment::VAlign_Center)
	;
}

TSharedRef<SWidget> FAvaPageIdColumn::ConstructRowWidget(const FAvaPageViewRef& InPageView
	, const TSharedPtr<SAvaPageViewRow>& InRow)
{
	return SNew(SBox)
		.Padding(UE::AvalanchePlaylist::FEditorMetrics::ColumnLeftOffset, 0.f, 0.f, 0.f)
		[
			SNew(SAvaPageId, InPageView)
		];
}

#undef LOCTEXT_NAMESPACE
