// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaPageNameColumn.h"

#include "Playlist/Pages/PageViews/AvaPageView.h"
#include "Widgets/Layout/SBox.h"
#include "Slate/SAvaPageName.h"

#define LOCTEXT_NAMESPACE "AvaPageNameColumn"

FText FAvaPageNameColumn::GetColumnDisplayNameText() const
{
	return LOCTEXT("PageName_Name", "Description");
}

FText FAvaPageNameColumn::GetColumnToolTipText() const
{
	return LOCTEXT("PageName_ToolTip", "Description of the given Page. Can be null or repeated");
}

SHeaderRow::FColumn::FArguments FAvaPageNameColumn::ConstructHeaderRowColumn()
{
	return SHeaderRow::Column(GetColumnId())
		.DefaultLabel(GetColumnDisplayNameText())
		.DefaultTooltip(GetColumnToolTipText())
		.FillWidth(0.25f)
		.ShouldGenerateWidget(true)
		.VAlignCell(EVerticalAlignment::VAlign_Center)
	;
}

TSharedRef<SWidget> FAvaPageNameColumn::ConstructRowWidget(const FAvaPageViewRef& InPageView
	, const TSharedPtr<SAvaPageViewRow>& InRow)
{
	return SNew(SBox)
		.Padding(UE::AvalanchePlaylist::FEditorMetrics::ColumnLeftOffset, 0.f, 0.f, 0.f)
		[
			SNew(SAvaPageName, InPageView, InRow)
		];
}

#undef LOCTEXT_NAMESPACE
