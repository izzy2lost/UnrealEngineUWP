// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "WorkflowOrientedApp/WorkflowTabFactory.h"

class FAvaPlaylistEditor;

//Base class for all Tab Factories in Ava Playlist Editor
class FAvaPlaylistTabFactory : public FWorkflowTabFactory
{
public:

	FAvaPlaylistTabFactory(const FName& InTabID, const TSharedPtr<FAvaPlaylistEditor>& InPlaylistEditor);

protected:

	TWeakPtr<FAvaPlaylistEditor> PlaylistEditorWeak;
};

