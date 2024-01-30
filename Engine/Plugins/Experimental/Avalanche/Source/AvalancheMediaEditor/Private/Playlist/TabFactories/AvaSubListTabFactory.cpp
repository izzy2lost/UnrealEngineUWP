// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaSubListTabFactory.h"
#include "Playlist/AvaPlaylistEditor.h"
#include "Playlist/Pages/Slate/SAvaSubListStartPage.h"

#define LOCTEXT_NAMESPACE "FAvaSubListTabFactory"

const FName FAvaSubListTabFactory::TabID(TEXT("AvalanchePlaylistSubList"));

FAvaSubListTabFactory::FAvaSubListTabFactory(const TSharedPtr<FAvaPlaylistEditor>& InPlaylistEditor)
	: FAvaPlaylistTabFactory(TabID, InPlaylistEditor)
{
	PlaylistEditorWeak = InPlaylistEditor;

	TabLabel = LOCTEXT("PlaylistSubList_TabLabel", "Page View Start");
	TabIcon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "PlacementBrowser.Icons.All");

	bIsSingleton = true;

	ViewMenuDescription = LOCTEXT("PlaylistSubList_ViewMenu_Desc", "Page View Start");
	ViewMenuTooltip = LOCTEXT("PlaylistSubList_ViewMenu_ToolTip", "Page View Start");
}

TSharedRef<SWidget> FAvaSubListTabFactory::CreateTabBody(const FWorkflowTabSpawnInfo& InInfo) const
{
	TSharedPtr<FAvaPlaylistEditor> PlaylistEditor = PlaylistEditorWeak.Pin();

	return SNew(SAvaSubListStartPage, PlaylistEditor);
}

#undef LOCTEXT_NAMESPACE
