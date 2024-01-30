// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaTemplatePageListTabFactory.h"
#include "Playlist/AvaPlaylistEditor.h"
#include "Playlist/Pages/Slate/SAvaTemplatePageList.h"

const FName FAvaTemplatePageListTabFactory::TabID(TEXT("AvalancheTemplatePlaylistPageList"));

#define LOCTEXT_NAMESPACE "AvaTemplatePageListTabFactory"

FAvaTemplatePageListTabFactory::FAvaTemplatePageListTabFactory(const TSharedPtr<FAvaPlaylistEditor>& InPlaylistEditor)
	: FAvaPlaylistTabFactory(TabID, InPlaylistEditor)
{
	TabLabel = LOCTEXT("PlaylistPageList_TabLabel", "Templates");
	TabIcon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "PlacementBrowser.Icons.All");

	bIsSingleton = true;

	ViewMenuDescription = LOCTEXT("PlaylistPageList_ViewMenu_Desc", "Templates");
	ViewMenuTooltip = LOCTEXT("PlaylistPageList_ViewMenu_ToolTip", "Templates");
}

TSharedRef<SWidget> FAvaTemplatePageListTabFactory::CreateTabBody(const FWorkflowTabSpawnInfo& InInfo) const
{
	return SNew(SAvaTemplatePageList, PlaylistEditorWeak.Pin());
}

#undef LOCTEXT_NAMESPACE
