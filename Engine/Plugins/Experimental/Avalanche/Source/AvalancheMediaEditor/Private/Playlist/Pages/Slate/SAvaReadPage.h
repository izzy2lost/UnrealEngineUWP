// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Playlist/AvaPlaylistDefines.h"
#include "Widgets/SCompoundWidget.h"

class FAvaPlaylistEditor;
class SAvaReadPageEditableTextBox;

class SAvaReadPage : public SCompoundWidget
{
public:
	
	SLATE_BEGIN_ARGS(SAvaReadPage) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TSharedPtr<FAvaPlaylistEditor> InPlaylistEditor);

	virtual ~SAvaReadPage() override;

	void OnPageEvent(const TArray<int32>& InSelectedPageIds, UE::AvalanchePlaylist::EPageEvent InPageEvent);

	bool IsKeyRelevant(const FKeyEvent& InKeyEvent) const;

	bool ProcessPlaylistKeyDown(const FKeyEvent& InKeyEvent);

	bool OnVerifyTextChanged(const FText& InText, FText& OutErrorMessage) const;

	void OnTextCommitted(const FText& InText, ETextCommit::Type InCommitType);

	FReply OnReadPageClicked();

private:
	TWeakPtr<FAvaPlaylistEditor> PlaylistEditorWeak;

	TSharedPtr<SAvaReadPageEditableTextBox> ReadPageText;
};
