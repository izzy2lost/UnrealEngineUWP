// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaType.h"
#include "CoreMinimal.h"
#include "Playlist/AvaPlaylistDefines.h"
#include "Widgets/Views/SHeaderRow.h"

class SAvaPageViewRow;

class IAvaPageViewColumn : public IAvaTypeCastable, public TSharedFromThis<IAvaPageViewColumn>
{
public:
	UE_AVA_INHERITS(IAvaPageViewColumn, IAvaTypeCastable);

	FName GetColumnId() const { return GetTypeId().ToName(); }

	virtual FText GetColumnDisplayNameText() const = 0;
	virtual FText GetColumnToolTipText() const = 0;

	virtual SHeaderRow::FColumn::FArguments ConstructHeaderRowColumn() = 0;
	
	virtual TSharedRef<SWidget> ConstructRowWidget(const FAvaPageViewRef& InPageView, const TSharedPtr<SAvaPageViewRow>& InRow) = 0;
};
