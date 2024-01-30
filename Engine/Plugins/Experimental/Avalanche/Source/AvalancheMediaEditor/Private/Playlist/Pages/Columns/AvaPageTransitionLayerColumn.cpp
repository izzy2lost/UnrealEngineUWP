// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaPageTransitionLayerColumn.h"
#include "Playlist/Pages/PageViews/AvaPageView.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "AvaPageTransitionLayerColumn"

FText FAvaPageTransitionLayerColumn::GetColumnDisplayNameText() const
{
	return LOCTEXT("TransitionLayerColumn_LayerName", "Layer");
}

FText FAvaPageTransitionLayerColumn::GetColumnToolTipText() const
{
	return LOCTEXT("TransitionLayerColumn_ToolTip", "Transition layer name for the page");
}

SHeaderRow::FColumn::FArguments FAvaPageTransitionLayerColumn::ConstructHeaderRowColumn()
{
	return SHeaderRow::Column(GetColumnId())
		.DefaultLabel(GetColumnDisplayNameText())
		.DefaultTooltip(GetColumnToolTipText())
		.FillWidth(0.25f)
		.ShouldGenerateWidget(true)
		.VAlignCell(EVerticalAlignment::VAlign_Center)
	;
}

TSharedRef<SWidget> FAvaPageTransitionLayerColumn::ConstructRowWidget(const FAvaPageViewRef& InPageView, const TSharedPtr<SAvaPageViewRow>& InRow)
{
	return SNew(SBox)
		.Padding(UE::AvalanchePlaylist::FEditorMetrics::ColumnLeftOffset, 0.f, 0.f, 0.f)
		[
			SNew(STextBlock)
			.Text(InPageView, &IAvaPageView::GetPageTransitionLayerNameText)
		];
}

#undef LOCTEXT_NAMESPACE
