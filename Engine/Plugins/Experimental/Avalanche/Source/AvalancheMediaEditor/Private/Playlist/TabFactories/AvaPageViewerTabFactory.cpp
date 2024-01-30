// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaPageViewerTabFactory.h"
#include "Playlist/AvaPlaylistEditor.h"
#include "Playlist/Preview/SAvaPagePreview.h"

const FName FAvaPageViewerTabFactory::TabID(TEXT("AvalanchePlaylistPageViewer"));

#define LOCTEXT_NAMESPACE "AvalanchePlaylist"

FAvaPageViewerTabFactory::FAvaPageViewerTabFactory(const TSharedPtr<FAvaPlaylistEditor>& InPlaylistEditor)
	: FAvaPlaylistTabFactory(TabID, InPlaylistEditor)
{
	TabLabel = LOCTEXT("PlaylistPageViewer_TabLabel", "Page Preview");
	TabIcon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.VirtualProduction");

	bIsSingleton = true;

	ViewMenuDescription = LOCTEXT("PlaylistPageViewer_ViewMenu_Desc", "Previews the selected page");
	ViewMenuTooltip = LOCTEXT("PlaylistPageViewer_ViewMenu_ToolTip", "Previews the selected page");
}

TSharedRef<SWidget> FAvaPageViewerTabFactory::CreateTabBody(const FWorkflowTabSpawnInfo& Info) const
{
	TSharedPtr<FAvaPlaylistEditor> PlaylistEditor = PlaylistEditorWeak.Pin();
	
	return SNew(SAvaPagePreview, PlaylistEditor);
}

#undef LOCTEXT_NAMESPACE
