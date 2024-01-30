// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaPlaylistTabFactory.h"
#include "Playlist/AvaPlaylistEditor.h"

FAvaPlaylistTabFactory::FAvaPlaylistTabFactory(const FName& InTabID, const TSharedPtr<FAvaPlaylistEditor>& InPlaylistEditor)
	: FWorkflowTabFactory(InTabID, InPlaylistEditor)
	, PlaylistEditorWeak(InPlaylistEditor)
{
}
