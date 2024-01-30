// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaPlaylistTabFactory.h"

class FAvaPlaylistEditor;

//Base class for all Tab Factories in Ava SubList Editor
class FAvaSubListTabFactory : public FAvaPlaylistTabFactory
{
public:
	static const FName TabID;

	FAvaSubListTabFactory(const TSharedPtr<FAvaPlaylistEditor>& InSubListEditor);

	virtual TSharedRef<SWidget> CreateTabBody(const FWorkflowTabSpawnInfo& InInfo) const override;

protected:
	TWeakPtr<FAvaPlaylistEditor> PlaylistEditorWeak;
};

