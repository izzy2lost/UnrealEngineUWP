// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaPlaybackDetailsTabFactory.h"
#include "Playback/AvalanchePlayback.h"
#include "Playback/AvaPlaybackEditor.h"
#include "Playback/DetailsView/SAvaPlaybackDetailsView.h"

const FName FAvaPlaybackDetailsTabFactory::TabID(TEXT("AvalanchePlaybackDetails"));

#define LOCTEXT_NAMESPACE "AvaPlaybackDetailsTabFactory"

FAvaPlaybackDetailsTabFactory::FAvaPlaybackDetailsTabFactory(const TSharedPtr<FAvaPlaybackEditor>& InPlaybackEditor)
	: FAvaPlaybackTabFactory(TabID, InPlaybackEditor)
{
	TabLabel = LOCTEXT("PlaybackDetails_TabLabel", "Details");
	TabIcon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Details");

	bIsSingleton = true;

	ViewMenuDescription = LOCTEXT("PlaybackDetails_ViewMenu_Desc", "Playback Node Details");
	ViewMenuTooltip = LOCTEXT("PlaybackDetails_ViewMenu_ToolTip", "Playback Node Details");
}

TSharedRef<SWidget> FAvaPlaybackDetailsTabFactory::CreateTabBody(const FWorkflowTabSpawnInfo& Info) const
{
	TSharedPtr<FAvaPlaybackEditor> PlaybackEditor = PlaybackEditorWeak.Pin();
	check(PlaybackEditor.IsValid());
	return SNew(SAvaPlaybackDetailsView, PlaybackEditor);
}

#undef LOCTEXT_NAMESPACE
