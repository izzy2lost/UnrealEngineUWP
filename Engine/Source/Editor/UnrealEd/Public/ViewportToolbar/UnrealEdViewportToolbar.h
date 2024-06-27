// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Delegates/DelegateCombinations.h"
#include "Engine/EngineBaseTypes.h"
#include "Templates/SharedPointerFwd.h"
#include "UnrealEdViewportToolbarContext.h"

class FText;
class SEditorViewport;
class UToolMenu;
struct FToolMenuEntry;

namespace UE::UnrealEd
{

/** The value of this function is controlled by the CVAR "ToolMenusViewportToolbars". */
UNREALED_API bool ShowOldViewportToolbars();

/** The value of this function is controlled by the CVAR "ToolMenusViewportToolbars". */
UNREALED_API bool ShowNewViewportToolbars();

UNREALED_API FToolMenuEntry CreateViewportToolbarTransformsSection();

UNREALED_API FToolMenuEntry CreateViewportToolbarSelectionSection();

UNREALED_API FText GetViewModesSubmenuLabel(TWeakPtr<SEditorViewport> InViewport);

/**
 * Populate a given UToolMenu with entries for a View Modes viewport toolbar submenu.
 *
 * @param InMenu The menu to poulate with entries.
 * @param InViewport The viewport associated with this viewport toolbar.
 * @param InIsViewModeSupported Optional delegate to filter which view modes are added to the list.
 */
UNREALED_API void PopulateViewModesMenu(UToolMenu* InMenu,
	TSharedRef<SEditorViewport> InViewport,
	IsViewModeSupportedDelegate InIsViewModeSupported = IsViewModeSupportedDelegate());

UNREALED_API FToolMenuEntry CreateViewportToolbarViewModesSubmenu();

} // namespace UE::UnrealEd
