// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Playlist/Pages/Columns/AvaPageViewColumn.h"

class FAvaPageNameColumn : public IAvaPageViewColumn
{
public:
	UE_AVA_INHERITS(FAvaPageNameColumn, IAvaPageViewColumn);

	//~ Begin IAvaPageViewColumn
	virtual FText GetColumnDisplayNameText() const override;
	virtual FText GetColumnToolTipText() const override;
	virtual SHeaderRow::FColumn::FArguments ConstructHeaderRowColumn() override;
	virtual TSharedRef<SWidget> ConstructRowWidget(const FAvaPageViewRef& InPageView, const TSharedPtr<SAvaPageViewRow>& InRow) override;
	//~ End IAvaPageViewColumn
};
