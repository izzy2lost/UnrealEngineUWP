// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaPlaybackGraphTabFactory.h"
#include "GraphEditor.h"
#include "Playback/AvalanchePlayback.h"
#include "Playback/AvaPlaybackEditor.h"

const FName FAvaPlaybackGraphTabFactory::TabID(TEXT("AvalanchePlaybackGraph"));

#define LOCTEXT_NAMESPACE "AvaPlaybackGraphTabFactory"

FAvaPlaybackGraphTabFactory::FAvaPlaybackGraphTabFactory(const TSharedPtr<FAvaPlaybackEditor>& InPlaybackEditor)
	: FAvaPlaybackTabFactory(TabID, InPlaybackEditor)
{
	TabLabel = LOCTEXT("PlaybackGraph_TabLabel", "Playback Graph");
	TabIcon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "GraphEditor.EventGraph_16x");

	bIsSingleton = true;

	ViewMenuDescription = LOCTEXT("PlaybackGraph_ViewMenu_Desc", "Playback Graph");
	ViewMenuTooltip = LOCTEXT("PlaybackGraph_ViewMenu_ToolTip", "Playback Graph");
}

TSharedRef<SWidget> FAvaPlaybackGraphTabFactory::CreateTabBody(const FWorkflowTabSpawnInfo& Info) const
{
	TSharedRef<FAvaPlaybackEditor> PlaybackEditor = PlaybackEditorWeak.Pin().ToSharedRef();	
	return PlaybackEditor->CreateGraphEditor();
}

#undef LOCTEXT_NAMESPACE
