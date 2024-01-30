// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaShowControlTabFactory.h"
#include "Playlist/AvaPlaylistEditor.h"
#include "Playlist/ShowControl/SAvaShowControl.h"

const FName FAvaShowControlTabFactory::TabID(TEXT("AvalanchePlaylistShowControl"));

#define LOCTEXT_NAMESPACE "AvaShowControlTabFactory"

FAvaShowControlTabFactory::FAvaShowControlTabFactory(const TSharedPtr<FAvaPlaylistEditor>& InPlaylistEditor)
	: FAvaPlaylistTabFactory(TabID, InPlaylistEditor)
{
	TabLabel = LOCTEXT("PlaylistShowControl_TabLabel", "Show Control");
	TabIcon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Toolbar.Settings");

	bIsSingleton = true;
	bShouldAutosize = false;

	ViewMenuDescription = LOCTEXT("PlaylistShowControl_ViewMenu_Desc", "Show Control");
	ViewMenuTooltip = LOCTEXT("PlaylistShowControl_ViewMenu_ToolTip", "Show Control");
}

TSharedRef<SWidget> FAvaShowControlTabFactory::CreateTabBody(const FWorkflowTabSpawnInfo& Info) const
{
	return SNew(SAvaShowControl, PlaylistEditorWeak.Pin());
}

#undef LOCTEXT_NAMESPACE
