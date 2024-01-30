// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Playlist/Pages/Columns/AvaPageViewColumn.h"

class FAvaPageAssetSelectorColumn : public IAvaPageViewColumn
{
public:
	UE_AVA_INHERITS(FAvaPageAssetSelectorColumn, IAvaPageViewColumn);

	//~ Begin IAvaPageViewColumn
	virtual FText GetColumnDisplayNameText() const override;
	virtual FText GetColumnToolTipText() const override;
	virtual SHeaderRow::FColumn::FArguments ConstructHeaderRowColumn() override;
	virtual TSharedRef<SWidget> ConstructRowWidget(const FAvaPageViewRef& InPageView, const TSharedPtr<SAvaPageViewRow>& InRow) override;
	//~ End IAvaPageViewColumn
};
