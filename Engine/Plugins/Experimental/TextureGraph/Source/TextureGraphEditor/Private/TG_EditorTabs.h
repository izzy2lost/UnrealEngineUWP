// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

class FName;

/**  TG_ editor tabs identifiers */
struct FTG_EditorTabs
{
	/**	The tab id for the TG_ fixture types tab */
	static const FName ViewportTabId;
	static const FName PropertiesTabId;
	static const FName PaletteTabId;
	static const FName FindTabId;
	static const FName GraphEditorId;
	static const FName PreviewSceneSettingsTabId;
	static const FName ParameterDefaultsTabId;
#if TEXTUREGRAPHEDITOR_ENABLE_OLD_SELECTION_PREVIEW
	static const FName SelectionPreviewTabId;
#endif // TEXTUREGRAPHEDITOR_ENABLE_OLD_SELECTION_PREVIEW
#if TEXTUREGRAPHEDITOR_ENABLE_NEW_NODE_PREVIEW
	static const FName NodePreviewTabId;
#endif // TEXTUREGRAPHEDITOR_ENABLE_NEW_NODE_PREVIEW
	static const FName OutputTabId;
	static const FName PreviewSettingsTabId;
	static const FName ErrorsTabId;
	static const FName TextureDetailsTabId;

	// Disable default constructor
	FTG_EditorTabs() = delete;
};