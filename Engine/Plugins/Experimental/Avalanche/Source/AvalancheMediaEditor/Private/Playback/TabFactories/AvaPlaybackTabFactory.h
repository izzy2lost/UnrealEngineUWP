// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "WorkflowOrientedApp/WorkflowTabFactory.h"

class FAvaPlaybackEditor;

//Base class for all Tab Factories in Ava Playback Editor
class FAvaPlaybackTabFactory : public FWorkflowTabFactory
{
public:
	
	FAvaPlaybackTabFactory(const FName& InTabID, const TSharedPtr<FAvaPlaybackEditor>& InPlaybackEditor);

protected:

	TWeakPtr<FAvaPlaybackEditor> PlaybackEditorWeak;
};
