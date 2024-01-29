// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Layout/Visibility.h"
#include "Misc/Attribute.h"
#include "Templates/SharedPointer.h"

class SDMEditor;
class SWidget;
class UToolMenu;
struct FToolMenuSection;
struct FUIAction;

class FDMToolBarMenus
{
public:
	static TSharedRef<SWidget> MakeEditorLayoutMenu(const TSharedPtr<SDMEditor>& InEditorWidget = nullptr);

	static void AddPreviewOptionsSection(UToolMenu* InMenu);
	static void AddTooltipOptionsSection(UToolMenu* InMenu);
	static void AddAdvancedSection(UToolMenu* InMenu);

	/** Helper functions to add property menu entries. */
	static void AddBoolOptionMenuEntry(FToolMenuSection& InSection, const FName& InPropertyName, const FUIAction InAction);
	static void AddIntOptionMenuEntry(FToolMenuSection& InSection, const FName& InPropertyName, TAttribute<bool> InIsEnabledAttribute = TAttribute<bool>(), TAttribute<EVisibility> InVisibilityAttribute = TAttribute<EVisibility>());
};
