// Copyright Epic Games, Inc. All Rights Reserved.

#include "StormSyncAvaBridgeEditor.h"

#include "StormSyncAvaPlaylistExtender.h"

void FStormSyncAvaBridgeEditorModule::StartupModule()
{
	PlaylistExtender = MakeShared<FStormSyncAvaPlaylistExtender>();
}

void FStormSyncAvaBridgeEditorModule::ShutdownModule()
{
	if (PlaylistExtender.IsValid())
	{
		PlaylistExtender.Reset();
	}
}

IMPLEMENT_MODULE(FStormSyncAvaBridgeEditorModule, StormSyncAvaBridgeEditor)
