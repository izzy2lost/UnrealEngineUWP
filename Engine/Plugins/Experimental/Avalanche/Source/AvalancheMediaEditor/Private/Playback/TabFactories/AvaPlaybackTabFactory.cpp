// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaPlaybackTabFactory.h"
#include "Playback/AvaPlaybackEditor.h"

FAvaPlaybackTabFactory::FAvaPlaybackTabFactory(const FName& InTabID, const TSharedPtr<FAvaPlaybackEditor>& InPlaybackEditor)
	: FWorkflowTabFactory(InTabID, InPlaybackEditor)
	, PlaybackEditorWeak(InPlaybackEditor)
{
}
