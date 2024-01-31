// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaPageEnabledColumn.h"
#include "Playlist/AvalanchePage.h"
#include "Playlist/Pages/PageViews/AvaInstancedPageViewImpl.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Layout/SBox.h"

#define LOCTEXT_NAMESPACE "AvaPageEnabledColumn"

FText FAvaPageEnabledColumn::GetColumnDisplayNameText() const
{
	return LOCTEXT("EnabledColumn_Name", "Enabled");
}

FText FAvaPageEnabledColumn::GetColumnToolTipText() const
{
	return LOCTEXT("EnabledColumn_ToolTip", "Determines whether this Page should be considered for Rundown");
}

SHeaderRow::FColumn::FArguments FAvaPageEnabledColumn::ConstructHeaderRowColumn()
{
	return SHeaderRow::Column(GetColumnId())
		.DefaultLabel(GetColumnDisplayNameText())
		.DefaultTooltip(GetColumnToolTipText())
		.FixedWidth(24.0f)
		.VAlignCell(EVerticalAlignment::VAlign_Center)
		.HAlignCell(EHorizontalAlignment::HAlign_Center)
		.ShouldGenerateWidget(true)
		[
			SNew(SCheckBox)
			.IsChecked(true)
		];
}

TSharedRef<SWidget> FAvaPageEnabledColumn::ConstructRowWidget(const FAvaPageViewRef& InPageView, const TSharedPtr<SAvaPageViewRow>& InRow)
{
	const TSharedRef<FAvaInstancedPageViewImpl> InstancedPageView = StaticCastSharedRef<FAvaInstancedPageViewImpl>(InPageView);

	return SNew(SBox)
		.Padding(UE::AvalanchePlaylist::FEditorMetrics::ColumnLeftOffset, 0.f, 0.f, 0.f)
		[
			SNew(SCheckBox)
			.IsChecked(InstancedPageView, &FAvaInstancedPageViewImpl::IsEnabled)
			.OnCheckStateChanged(InstancedPageView, &FAvaInstancedPageViewImpl::SetEnabled)
		];
}

#undef LOCTEXT_NAMESPACE
