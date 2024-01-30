// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaPageDetailsTabFactory.h"
#include "Playlist/AvaPlaylistEditor.h"
#include "Playlist/DetailsView/SAvaPageDetails.h"

const FName FAvaPageDetailsTabFactory::TabID(TEXT("AvalanchePlaylistPageDetails"));

#define LOCTEXT_NAMESPACE "AvaPageDetailsTabFactory"

FAvaPageDetailsTabFactory::FAvaPageDetailsTabFactory(const TSharedPtr<FAvaPlaylistEditor>& InPlaylistEditor)
	: FAvaPlaylistTabFactory(TabID, InPlaylistEditor)
{
	TabLabel = LOCTEXT("PlaylistPageDetails_TabLabel", "Page Details");
	TabIcon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Details");

	bIsSingleton = true;

	ViewMenuDescription = LOCTEXT("PlaylistPageDetails_ViewMenu_Desc", "Page Details");
	ViewMenuTooltip = LOCTEXT("PlaylistPageDetails_ViewMenu_ToolTip", "Page Details");
}

TSharedRef<SWidget> FAvaPageDetailsTabFactory::CreateTabBody(const FWorkflowTabSpawnInfo& Info) const
{
	return SNew(SAvaPageDetails, PlaylistEditorWeak.Pin());
}

#undef LOCTEXT_NAMESPACE
