// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaPlaylistAppMode.h"
#include "Playlist/AvaPlaylistEditor.h"

#define LOCTEXT_NAMESPACE "AvaPlaylistAppMode"

const FName FAvaPlaylistAppMode::DefaultMode(TEXT("DefaultMode"));

FAvaPlaylistAppMode::FAvaPlaylistAppMode(const TSharedPtr<FAvaPlaylistEditor>& InPlaylistEditor, const FName& InModeName)
	: FApplicationMode(InModeName, FAvaPlaylistAppMode::GetLocalizedMode)
	, PlaylistEditorWeak(InPlaylistEditor)
{
}

void FAvaPlaylistAppMode::RegisterTabFactories(TSharedPtr<FTabManager> InTabManager)
{
	TSharedPtr<FAvaPlaylistEditor> PlaybackEditor = PlaylistEditorWeak.Pin();
	PlaybackEditor->PushTabFactories(TabFactories);
	FApplicationMode::RegisterTabFactories(InTabManager);
}

TSharedPtr<FDocumentTabFactory> FAvaPlaylistAppMode::GetDocumentTabFactory(const FName& InName) const
{
	if (const TSharedRef<FDocumentTabFactory>* DocFactory = DocumentTabFactories.Find(InName))
	{
		return *DocFactory;
	}

	return nullptr;
}

FText FAvaPlaylistAppMode::GetLocalizedMode(const FName InMode)
{
	static TMap<FName, FText> LocModes;

	if (LocModes.Num() == 0)
	{
		LocModes.Add(DefaultMode, LOCTEXT("Playlist_DefaultMode", "Default"));
	}

	check(InMode != NAME_None);
	const FText* OutDesc = LocModes.Find(InMode);
	check(OutDesc);
	
	return *OutDesc;
}

#undef LOCTEXT_NAMESPACE
