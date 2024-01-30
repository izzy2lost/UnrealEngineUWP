// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/NameTypes.h"
#include "UObject/WeakObjectPtr.h"

class FAvaPlaylistEditor;
class UToolMenu;
class UAvalanchePlaylist;

class FAvaTransitionPlaylistExtension
{
public:
	FAvaTransitionPlaylistExtension();

	~FAvaTransitionPlaylistExtension();

	void Startup();

	void Shutdown();

private:
	void ExtendPageContextMenu(UToolMenu* InMenu);

	void OpenTransitionTree(TWeakPtr<FAvaPlaylistEditor> InPlaylistEditorWeak);

	void OpenTransitionTree(const FSoftObjectPath& InPageAssetPath, const TCHAR* InPlaylistName, int32 InPageId);

	TWeakObjectPtr<UToolMenu> PageContextMenuWeak;

	const FName ExtensionSectionName;
};
