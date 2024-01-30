// Copyright Epic Games, Inc. All Rights Reserved.

#include "SAvaSubListStartPage.h"

#include "Playlist/AvaPlaylistEditor.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SAvaSubListStartPage"

void SAvaSubListStartPage::Construct(const FArguments& InArgs, TSharedPtr<FAvaPlaylistEditor> InPlaylistEditor)
{
	PlaylistEditorWeak = InPlaylistEditor;

	ChildSlot
	[
		SNew(SBox)
		.Padding(10.f, 10.f)
		.HAlign(EHorizontalAlignment::HAlign_Center)
		.VAlign(EVerticalAlignment::VAlign_Top)
		[
			SNew(SButton)
			.ToolTipText(LOCTEXT("AddSubListTooltip", "Add Page View"))
			.OnClicked(this, &SAvaSubListStartPage::OnCreateSubListClicked)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("AddSubList", "Add Page View"))
			]
		]
	];
}

FReply SAvaSubListStartPage::OnCreateSubListClicked()
{
	TSharedPtr<FAvaPlaylistEditor> PlaylistEditor = PlaylistEditorWeak.Pin();

	if (PlaylistEditor.IsValid())
	{
		UAvalanchePlaylist* Playlist = PlaylistEditor->GetPlaylist();

		if (IsValid(Playlist))
		{
			Playlist->AddSubList();

			return FReply::Handled();
		}
	}

	return FReply::Unhandled();
}

#undef LOCTEXT_NAMESPACE
