// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaMediaDefines.h"
#include "Templates/SharedPointer.h"
#include "UObject/Object.h"
#include "AvaPlaylistPageContext.generated.h"

class FAvaPlaylistEditor;
class FAvaPlaylistPageContextMenu;

UCLASS(MinimalAPI)
class UAvaPlaylistPageContext : public UObject
{
	GENERATED_BODY()

	friend FAvaPlaylistPageContextMenu;

public:
	TSharedPtr<FAvaPlaylistEditor> GetPlaylistEditor() const
	{
		return PlaylistEditorWeak.Pin();
	}

	const FAvaPageListReference& GetPageListReference() const
	{
		return PageListReference;
	}

private:
	TWeakPtr<FAvaPlaylistPageContextMenu> ContextMenuWeak;

	TWeakPtr<FAvaPlaylistEditor> PlaylistEditorWeak;

	FAvaPageListReference PageListReference;
};
