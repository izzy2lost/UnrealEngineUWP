// Copyright Epic Games, Inc. All Rights Reserved.


#pragma once


#include "CoreMinimal.h"
#include "Widgets/SWidget.h"
#include "SLevelEditor.h"

/**
 * Level editor menu
 */
class FLevelEditorMenu
{

public:
	static void RegisterLevelEditorMenus();

	/**
	 * Static: Creates a main menu for the given level editor's tab manager.
	 */
	static void MakeLevelEditorMenu(const TSharedPtr<FUICommandList>& CommandList, TSharedPtr<class SLevelEditor> LevelEditor);

private:
	static void RegisterBuildMenu();
	static void RegisterSelectMenu();
};
