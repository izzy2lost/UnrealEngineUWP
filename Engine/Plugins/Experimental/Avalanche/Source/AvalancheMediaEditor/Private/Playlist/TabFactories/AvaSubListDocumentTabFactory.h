// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "WorkflowOrientedApp/WorkflowTabFactory.h"

class FAvaPlaylistEditor;

/** Base class for all Tab Factories in Ava SubListDocument Editor */
class FAvaSubListDocumentTabFactory : public FDocumentTabFactory
{
public:
	static const FName FactoryId;
	static const FString BaseTabName;

	static FName GetTabId(int32 InSubListIndex);

	FAvaSubListDocumentTabFactory(const TSharedPtr<FAvaPlaylistEditor>& InSubListDocumentEditor);

	virtual TSharedRef<SWidget> CreateTabBody(const FWorkflowTabSpawnInfo& InInfo) const override;

	TSharedRef<SDockTab> SpawnSubListTab(const FWorkflowTabSpawnInfo& InInfo, int32 InSubListIndex);

protected:
	TWeakPtr<FAvaPlaylistEditor> PlaylistEditorWeak;
	int32 SubListIndex;
};

