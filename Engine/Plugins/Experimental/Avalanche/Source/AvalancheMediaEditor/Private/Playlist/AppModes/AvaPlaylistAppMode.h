// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "WorkflowOrientedApp/ApplicationMode.h"
#include "WorkflowOrientedApp/WorkflowTabManager.h"

class FAvaPlaylistEditor;

class FAvaPlaylistAppMode : public FApplicationMode
{
public:

	// Mode constants
	static const FName DefaultMode;
	//Add more here
	
	FAvaPlaylistAppMode(const TSharedPtr<FAvaPlaylistEditor>& InPlaylistEditor, const FName& InModeName);
	virtual void RegisterTabFactories(TSharedPtr<FTabManager> InTabManager) override;

	TSharedPtr<FDocumentTabFactory> GetDocumentTabFactory(const FName& InName) const;

protected:
	
	static FText GetLocalizedMode(const FName InMode);

	TWeakPtr<FAvaPlaylistEditor> PlaylistEditorWeak;
	
	// Set of spawnable tabs in the mode
	FWorkflowAllowedTabSet TabFactories;

	TMap<FName, TSharedRef<FDocumentTabFactory>> DocumentTabFactories;
};
