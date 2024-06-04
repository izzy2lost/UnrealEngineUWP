// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Delegates/DelegateCombinations.h"
#include "Engine/EngineBaseTypes.h"
#include "Templates/SharedPointerFwd.h"

class FText;
class SEditorViewport;
class UToolMenu;

namespace UE::UnrealEd
{

UNREALED_API FText GetViewModesSubmenuLabel(TWeakPtr<SEditorViewport> InViewport);

DECLARE_DELEGATE_RetVal_OneParam(bool, IsViewModeSupportedDelegate, EViewModeIndex);

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

} // namespace UE::UnrealEd
