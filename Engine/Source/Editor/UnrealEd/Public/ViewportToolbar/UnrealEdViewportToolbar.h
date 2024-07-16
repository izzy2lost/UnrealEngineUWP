// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Delegates/DelegateCombinations.h"
#include "Engine/EngineBaseTypes.h"
#include "Templates/SharedPointerFwd.h"
#include "ToolMenuDelegates.h"
#include "UnrealEdViewportToolbarContext.h"

class IPreviewProfileController;
class FEditorViewportClient;
class FText;
class SEditorViewport;
class UToolMenu;
struct FNewToolMenuChoice;
struct FToolMenuEntry;
enum ERotationGridMode : int;

namespace UE::UnrealEd
{

/** The value of this function is controlled by the CVAR "ToolMenusViewportToolbars". */
UNREALED_API bool ShowOldViewportToolbars();

/** The value of this function is controlled by the CVAR "ToolMenusViewportToolbars". */
UNREALED_API bool ShowNewViewportToolbars();

UNREALED_API FToolMenuEntry CreateViewportToolbarTransformsSection();

UNREALED_API FToolMenuEntry CreateViewportToolbarSelectionSection();

UNREALED_API FToolMenuEntry CreateViewportToolbarSnappingSubmenu();

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

DECLARE_DELEGATE_TwoParams(FRotationGridCheckboxListExecuteActionDelegate, int, ERotationGridMode);
DECLARE_DELEGATE_RetVal_TwoParams(bool, FRotationGridCheckboxListIsCheckedDelegate, int, ERotationGridMode);

DECLARE_DELEGATE_OneParam(FLocationGridCheckboxListExecuteActionDelegate, int);
DECLARE_DELEGATE_RetVal_OneParam(bool, FLocationGridCheckboxListIsCheckedDelegate, int);

DECLARE_DELEGATE_OneParam(FScaleGridCheckboxListExecuteActionDelegate, int);
DECLARE_DELEGATE_RetVal_OneParam(bool, FScaleGridCheckboxListIsCheckedDelegate, int);

DECLARE_DELEGATE_OneParam(FNumericEntryExecuteActionDelegate, float);

UNREALED_API TSharedRef<SWidget> BuildRotationGridCheckBoxList(
	FName InExtentionHook,
	const FText& InHeading,
	const TArray<float>& InGridSizes,
	ERotationGridMode InGridMode,
	const FRotationGridCheckboxListExecuteActionDelegate& InExecuteAction,
	const FRotationGridCheckboxListIsCheckedDelegate& InIsActionChecked,
	const TSharedPtr<FUICommandList>& InCommandList = {}
);

UNREALED_API TSharedRef<SWidget> CreateRotationGridSnapMenu(
	const FRotationGridCheckboxListExecuteActionDelegate& InExecuteDelegate,
	const FRotationGridCheckboxListIsCheckedDelegate& InIsCheckedDelegate,
	const TAttribute<bool>& InIsEnabledDelegate = TAttribute<bool>(true),
	const TSharedPtr<FUICommandList>& InCommandList = {}
);

UNREALED_API TSharedRef<SWidget> CreateLocationGridSnapMenu(
	const FLocationGridCheckboxListExecuteActionDelegate& InExecuteDelegate,
	const FLocationGridCheckboxListIsCheckedDelegate& InIsCheckedDelegate,
	const TArray<float>& InGridSizes,
	const TAttribute<bool>& InIsEnabledDelegate = TAttribute<bool>(true),
	const TSharedPtr<FUICommandList>& InCommandList = {}
);

UNREALED_API TSharedRef<SWidget> CreateScaleGridSnapMenu(
	const FScaleGridCheckboxListExecuteActionDelegate& InExecuteDelegate,
	const FScaleGridCheckboxListIsCheckedDelegate& InIsCheckedDelegate,
	const TArray<float>& InGridSizes,
	const TAttribute<bool>& InIsEnabledDelegate = TAttribute<bool>(true),
	const TSharedPtr<FUICommandList>& InCommandList = {},
	const TAttribute<bool>& ShowPreserveNonUniformScaleOption = TAttribute<bool>(false),
	const FUIAction& PreserveNonUniformScaleUIAction = FUIAction()
);

UNREALED_API FToolMenuEntry CreateCheckboxSubmenu(
	const FName InName,
	const TAttribute<FText>& InLabel,
	const TAttribute<FText>& InToolTip,
	const FToolMenuExecuteAction& InCheckboxExecuteAction,
	const FToolMenuCanExecuteAction& InCheckboxCanExecuteAction,
	const FToolMenuGetActionCheckState& InCheckboxActionCheckState,
	const FNewToolMenuChoice& InMakeMenu
);

UNREALED_API FToolMenuEntry CreateNumericEntry(
	const FName InName,
	const FText& InLabel,
	const FText& InTooltip,
	const FCanExecuteAction& InCanExecuteAction,
	const FNumericEntryExecuteActionDelegate& InOnValueChanged,
	const TAttribute<float>& InGetValue,
	float InMinValue = 0.0f,
	float InMaxValue = 1.0f,
	int32 InMaxFractionalDigits = 2
);

UNREALED_API FToolMenuEntry CreateCameraSubmenu(TWeakPtr<SEditorViewport> InViewport);

UNREALED_API FToolMenuEntry CreatePerformanceAndScalabilitySubmenu(TWeakPtr<SEditorViewport> InViewport);

// Camera Menu Widgets
TSharedRef<SWidget> CreateCameraMenuWidget(const TSharedRef<SEditorViewport>& InViewport);
TSharedRef<SWidget> CreateFOVMenuWidget(const TSharedRef<SEditorViewport>& InViewport);
TSharedRef<SWidget> CreateFarViewPlaneMenuWidget(const TSharedRef<SEditorViewport>& InViewport);

// Screen Percentage Submenu Widgets
TSharedRef<SWidget> CreateCurrentPercentageWidget(FEditorViewportClient& InViewportClient);
TSharedRef<SWidget> CreateResolutionsWidget(FEditorViewportClient& InViewportClient);
TSharedRef<SWidget> CreateActiveViewportWidget(FEditorViewportClient& InViewportClient);
TSharedRef<SWidget> CreateSetFromWidget(FEditorViewportClient& InViewportClient);
TSharedRef<SWidget> CreateCurrentScreenPercentageSettingWidget(FEditorViewportClient& InViewportClient);
TSharedRef<SWidget> CreateCurrentScreenPercentageWidget(FEditorViewportClient& InViewportClient);

} // namespace UE::UnrealEd
