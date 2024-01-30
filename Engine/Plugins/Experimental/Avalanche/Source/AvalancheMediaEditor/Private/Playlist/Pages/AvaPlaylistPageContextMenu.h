// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"

class FAvaPlaylistEditor;
class FUICommandList;
class SWidget;
class UAvaPlaylistPageContext;
class UToolMenu;
struct FAvaPageListReference;

class FAvaPlaylistPageContextMenu : public TSharedFromThis<FAvaPlaylistPageContextMenu>
{
public:
	TSharedRef<SWidget> GeneratePageContextMenuWidget(const TWeakPtr<FAvaPlaylistEditor>& InPlaylistEditorWeak, const FAvaPageListReference& InPageListReference, const TSharedPtr<FUICommandList>& InCommandList);

private:
	void PopulatePageContextMenu(UToolMenu& InMenu, UAvaPlaylistPageContext& InContext);
};
