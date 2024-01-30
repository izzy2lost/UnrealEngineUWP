// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Playlist/Pages/Columns/AvaPageViewColumn.h"

class FAvaTemplatePageViewImpl;

class FAvaPageTemplateStatusColumn : public IAvaPageViewColumn
{
public:
	UE_AVA_INHERITS(FAvaPageTemplateStatusColumn, IAvaPageViewColumn);

	//~ Begin IAvaPageViewColumn
	virtual FText GetColumnDisplayNameText() const override;
	virtual FText GetColumnToolTipText() const override;
	virtual SHeaderRow::FColumn::FArguments ConstructHeaderRowColumn() override;
	virtual TSharedRef<SWidget> ConstructRowWidget(const FAvaPageViewRef& InPageView, const TSharedPtr<SAvaPageViewRow>& InRow) override;
	//~ End IAvaPageViewColumn

	static FSlateColor GetIsPreviewingButtonColor(const TWeakPtr<FAvaTemplatePageViewImpl> InPageViewWeak);

	static FSlateColor GetAssetStatusButtonColor(const TWeakPtr<FAvaTemplatePageViewImpl> InPageViewWeak);
	static FText GetAssetStatusButtonTooltip(const TWeakPtr<FAvaTemplatePageViewImpl> InPageViewWeak);

	static FText GetSyncStatusButtonTooltip(const TWeakPtr<FAvaTemplatePageViewImpl> InPageViewWeak);
	static FSlateColor GetSyncStatusButtonColor(const TWeakPtr<FAvaTemplatePageViewImpl> InPageViewWeak);
};
