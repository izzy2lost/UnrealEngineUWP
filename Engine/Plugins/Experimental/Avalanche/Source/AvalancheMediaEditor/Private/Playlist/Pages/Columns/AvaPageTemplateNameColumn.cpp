// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaPageTemplateNameColumn.h"
#include "Playlist/Pages/PageViews/AvaInstancedPageViewImpl.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "AvaPageTemplateNameColumn"

FText FAvaPageTemplateNameColumn::GetColumnDisplayNameText() const
{
	return LOCTEXT("PageId_TemplateName", "Template");
}

FText FAvaPageTemplateNameColumn::GetColumnToolTipText() const
{
	return LOCTEXT("PageId_ToolTip", "Name and Id of the template used to create this page.");
}

SHeaderRow::FColumn::FArguments FAvaPageTemplateNameColumn::ConstructHeaderRowColumn()
{
	return SHeaderRow::Column(GetColumnId())
		.DefaultLabel(GetColumnDisplayNameText())
		.DefaultTooltip(GetColumnToolTipText())
		.FillWidth(0.1f)
		.ShouldGenerateWidget(true)
		.VAlignCell(EVerticalAlignment::VAlign_Center)
		;
}

TSharedRef<SWidget> FAvaPageTemplateNameColumn::ConstructRowWidget(const FAvaPageViewRef& InPageView
	, const TSharedPtr<SAvaPageViewRow>& InRow)
{
	const TSharedRef<FAvaInstancedPageViewImpl> InstancedPageView = StaticCastSharedRef<FAvaInstancedPageViewImpl>(InPageView);

	return SNew(SBox)
		.Padding(UE::AvalanchePlaylist::FEditorMetrics::ColumnLeftOffset, 0.f, 0.f, 0.f)
		[
			SNew(STextBlock)
			.Text(InstancedPageView, &FAvaInstancedPageViewImpl::GetTemplateDescription)
		];
}

#undef LOCTEXT_NAMESPACE
