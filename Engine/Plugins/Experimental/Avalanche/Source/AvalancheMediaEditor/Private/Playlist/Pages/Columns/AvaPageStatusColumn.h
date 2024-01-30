// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Playlist/Pages/Columns/AvaPageViewColumn.h"

class FAvaInstancedPageViewImpl;

class FAvaPageStatusColumn : public IAvaPageViewColumn
{
public:
	UE_AVA_INHERITS(FAvaPageStatusColumn, IAvaPageViewColumn);

	//~ Begin IAvaPageViewColumn
	virtual FText GetColumnDisplayNameText() const override;
	virtual FText GetColumnToolTipText() const override;
	virtual SHeaderRow::FColumn::FArguments ConstructHeaderRowColumn() override;
	virtual TSharedRef<SWidget> ConstructRowWidget(const FAvaPageViewRef& InPageView, const TSharedPtr<SAvaPageViewRow>& InRow) override;
	//~ End IAvaPageViewColumn

	static FSlateColor GetIsPreviewingButtonColor(const TWeakPtr<FAvaInstancedPageViewImpl> InPageViewWeak);

	static const FSlateBrush* GetPreviewBorderBackgroundImage(const TWeakPtr<FAvaInstancedPageViewImpl> InPageViewWeak);
	
	static FSlateColor GetIsPlayingButtonColor(const TWeakPtr<FAvaInstancedPageViewImpl> InPageViewWeak);

	static const FSlateBrush* GetProgramBorderBackgroundImage(const TWeakPtr<FAvaInstancedPageViewImpl> InPageViewWeak);

	static FText GetTakeInTooltip(const TWeakPtr<FAvaInstancedPageViewImpl> InPageViewWeak);

	static FSlateColor GetAssetStatusButtonColor(const TWeakPtr<FAvaInstancedPageViewImpl> InPageViewWeak);
	static FText GetAssetStatusButtonTooltip(const TWeakPtr<FAvaInstancedPageViewImpl> InPageViewWeak);
};
