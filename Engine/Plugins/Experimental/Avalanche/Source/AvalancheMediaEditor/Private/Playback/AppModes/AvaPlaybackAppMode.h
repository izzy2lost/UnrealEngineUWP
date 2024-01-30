// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "WorkflowOrientedApp/ApplicationMode.h"
#include "WorkflowOrientedApp/WorkflowTabManager.h"

class FAvaPlaybackEditor;

class FAvaPlaybackAppMode : public FApplicationMode
{
public:

	// Mode constants
	static const FName DefaultMode;
	//Add more here
	
	FAvaPlaybackAppMode(const TSharedPtr<FAvaPlaybackEditor>& InPlaybackEditor, const FName& InModeName);
	virtual void RegisterTabFactories(TSharedPtr<FTabManager> InTabManager) override;

protected:
	
	static FText GetLocalizedMode(const FName InMode);

	TWeakPtr<FAvaPlaybackEditor> PlaybackEditorWeak;
	
	// Set of spawnable tabs in the mode
	FWorkflowAllowedTabSet TabFactories;
};
