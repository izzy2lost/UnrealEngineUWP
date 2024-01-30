// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class FUICommandList;
class FAvaPlaylistEditor;

class SAvaShowControl : public SCompoundWidget
{
public:	
	SLATE_BEGIN_ARGS(SAvaShowControl) {}
	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs, const TSharedPtr<FAvaPlaylistEditor>& InPlaylistEditor);

	TSharedRef<SWidget> BuildShowControlToolBar(const TSharedRef<FUICommandList>& InCommandList);

protected:	
	TSharedPtr<FAvaPlaylistEditor> PlaylistEditor;

	TSharedRef<SWidget> CreateActiveListWidget();
	FText GetActiveListName() const;

	TSharedRef<SWidget> CreateNextPageWidget();
	FText GetNextPageName() const;
};
