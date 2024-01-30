// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"

class FAvaPlaylistEditor;

class SAvaSubListStartPage : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SAvaSubListStartPage) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TSharedPtr<FAvaPlaylistEditor> InPlaylistEditor);
	virtual ~SAvaSubListStartPage() override {}

protected:
	TWeakPtr<FAvaPlaylistEditor> PlaylistEditorWeak;

	FReply OnCreateSubListClicked();
};
