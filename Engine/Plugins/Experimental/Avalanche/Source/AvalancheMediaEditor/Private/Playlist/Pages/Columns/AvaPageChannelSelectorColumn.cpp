// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaPageChannelSelectorColumn.h"
#include "AvaBlueprint.h"
#include "PropertyCustomizationHelpers.h"
#include "Slate/SAvaPageChannelSelector.h"

#define LOCTEXT_NAMESPACE "AvaPageChannelSelectorColumn"

FText FAvaPageChannelSelectorColumn::GetColumnDisplayNameText() const
{
	return LOCTEXT("ChannelSelectorColumn_ChannelName", "Channel");
}

FText FAvaPageChannelSelectorColumn::GetColumnToolTipText() const
{
	return LOCTEXT("ChannelSelectorColumn_ToolTip", "Selects a Broadcast Channel for the Page");
}

SHeaderRow::FColumn::FArguments FAvaPageChannelSelectorColumn::ConstructHeaderRowColumn()
{
	return SHeaderRow::Column(GetColumnId())
		.DefaultLabel(GetColumnDisplayNameText())
		.DefaultTooltip(GetColumnToolTipText())
		.FillWidth(0.25f)
		.ShouldGenerateWidget(true)
		.VAlignCell(EVerticalAlignment::VAlign_Center)
	;
}

TSharedRef<SWidget> FAvaPageChannelSelectorColumn::ConstructRowWidget(const FAvaPageViewRef& InPageView, const TSharedPtr<SAvaPageViewRow>& InRow)
{
	return SNew(SBox)
		.Padding(UE::AvalanchePlaylist::FEditorMetrics::ColumnLeftOffset, 0.f, 0.f, 0.f)
		[
			SNew(SAvaPageChannelSelector, InPageView, InRow)
		];
}

#undef LOCTEXT_NAMESPACE
