// Copyright Epic Games, Inc. All Rights Reserved.

#include "ViewportToolbar/UnrealEdViewportToolbar.h"

#include "DataDrivenShaderPlatformInfo.h"
#include "DebugViewModeHelpers.h"
#include "Editor/EditorPerformanceSettings.h"
#include "EditorViewportClient.h"
#include "EditorViewportCommands.h"
#include "Framework/Commands/GenericCommands.h"
#include "GPUSkinCache.h"
#include "GPUSkinCacheVisualizationMenuCommands.h"
#include "IPreviewProfileController.h"
#include "LevelEditorActions.h"
#include "PreviewProfileController.h"
#include "RayTracingDebugVisualizationMenuCommands.h"
#include "SAssetEditorViewport.h"
#include "SEditorViewport.h"
#include "Settings/EditorProjectSettings.h"
#include "Settings/LevelEditorViewportSettings.h"
#include "Styling/SlateIconFinder.h"
#include "Templates/SharedPointer.h"
#include "ToolMenu.h"
#include "ToolMenuEntry.h"
#include "ToolMenuSection.h"
#include "ViewportToolbar/UnrealEdViewportToolbarContext.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/SToolTip.h"
#include "Widgets/Input/STextComboBox.h"

#define LOCTEXT_NAMESPACE "UnrealEdViewportToolbar"

namespace UE::UnrealEd::Private
{
int32 CVarToolMenusViewportToolbarsValue = 0;

FToolUIAction DisabledAction()
{
	static FToolUIAction Action;
	Action.CanExecuteAction = FToolMenuCanExecuteAction::CreateLambda(
		[](const FToolMenuContext&)
		{
			return false;
		}
	);

	return Action;
}

// TODO: Maybe export CreateSurfaceSnapOffsetEntry function, so that it can be used elsewhere, e.g. STransformViewportToolbar.cpp
FToolMenuEntry CreateSurfaceSnapOffsetEntry()
{
	FText Label = LOCTEXT("SurfaceOffsetLabel", "Surface Offset");
	FText Tooltip = LOCTEXT("SurfaceOffsetTooltip", "The amount of offset to apply when snapping to surfaces");

	const FMargin WidgetsMargin(2.0f, 0.0f, 3.0f, 0.0f);

	FToolMenuEntry SurfaceOffset = FToolMenuEntry::InitMenuEntry(
		"SurfaceOffset",
		FUIAction(
			FExecuteAction(),
			FCanExecuteAction::CreateLambda(
				[]()
				{
					return GetDefault<ULevelEditorViewportSettings>()->SnapToSurface.bEnabled;
				}
			)
		),
		// clang-format off
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Left)
		.Padding(WidgetsMargin)
		.AutoWidth()
		[
			SNew(STextBlock)
			.Text(Label)
		]
		+ SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		.Padding(WidgetsMargin)
		.AutoWidth()
		[
			SNew(SBox)
			.Padding(WidgetsMargin)
			.MinDesiredWidth(100.0f)
			[
				// Min/Max/Slider values taken from STransformViewportToolbar.cpp
				SNew(SNumericEntryBox<float>)
				.ToolTipText(Tooltip)
				.MinValue(0.0f)
				.MaxValue(static_cast<float>(HALF_WORLD_MAX))
				.MaxSliderValue(1000.0f)
				.AllowSpin(true)
				.MaxFractionalDigits(2)
				.Font(FAppStyle::GetFontStyle(TEXT("MenuItem.Font")))
				.OnValueChanged_Lambda([](float InNewValue)
				{
					ULevelEditorViewportSettings* Settings =
						GetMutableDefault<ULevelEditorViewportSettings>();
					Settings->SnapToSurface.SnapOffsetExtent = InNewValue;
				})
				.Value_Lambda([]()
				{
					return GetDefault<ULevelEditorViewportSettings>()->SnapToSurface.SnapOffsetExtent;
				})
			]
		]
		// clang-format on
	);

	return SurfaceOffset;
}

FToolMenuEntry CreateSurfaceSnapCheckboxMenu()
{
	FNewToolMenuDelegate MakeMenuDelegate = FNewToolMenuDelegate::CreateLambda(
		[](UToolMenu* Submenu)
		{
			FToolMenuSection& SurfaceSnappingSection =
				Submenu->FindOrAddSection("SurfaceSnapping", LOCTEXT("SurfaceSnappingLabel", "Surface Snapping"));

			// Add "Rotate to surface normal" checkbox.
			{
				FToolMenuEntry RotateToSurfaceNormalSnapping =
					FToolMenuEntry::InitMenuEntry(FEditorViewportCommands::Get().RotateToSurfaceNormal);
				SurfaceSnappingSection.AddEntry(RotateToSurfaceNormalSnapping);
			}

			// Add "Surface offset" widget.
			{
				SurfaceSnappingSection.AddEntry(CreateSurfaceSnapOffsetEntry());
			}
		}
	);

	return UnrealEd::CreateCheckboxSubmenu(
		"SurfaceSnapping",
		LOCTEXT("SurfaceSnapLabel", "Surface"),
		FEditorViewportCommands::Get().SurfaceSnapping->MakeTooltip()->GetTextTooltip(),
		FToolMenuExecuteAction::CreateLambda(
			[](const FToolMenuContext& InContext)
			{
				ULevelEditorViewportSettings* Settings = GetMutableDefault<ULevelEditorViewportSettings>();
				Settings->SnapToSurface.bEnabled = !Settings->SnapToSurface.bEnabled;
			}
		),
		FToolMenuCanExecuteAction(),
		FToolMenuGetActionCheckState::CreateLambda(
			[](const FToolMenuContext& InContext)
			{
				return GetDefault<ULevelEditorViewportSettings>()->SnapToSurface.bEnabled ? ECheckBoxState::Checked
																						  : ECheckBoxState::Unchecked;
			}
		),
		MakeMenuDelegate
	);
}

FToolMenuEntry CreateActorSnapDistanceEntry()
{
	const FText Label = LOCTEXT("ActorSnapDistanceLabel", "Snap Distance");
	const FText Tooltip = LOCTEXT("ActorSnapDistanceTooltip", "The amount of offset to apply when snapping to surfaces");

	const FMargin WidgetsMargin(2.0f, 0.0f, 3.0f, 0.0f);

	FToolMenuEntry SnapDistance = FToolMenuEntry::InitMenuEntry(
		"ActorSnapDistance",
		FUIAction(
			FExecuteAction(),
			FCanExecuteAction::CreateLambda(
				[]()
				{
					return !!GetDefault<ULevelEditorViewportSettings>()->bEnableActorSnap;
				}
			)
		),
		// clang-format off
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		.Padding(WidgetsMargin)
		.AutoWidth()
		[
			SNew(STextBlock)
			.Text(Label)
		]
		+ SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		.Padding(WidgetsMargin)
		.AutoWidth()
		[
			SNew(SBox).Padding(WidgetsMargin).MinDesiredWidth(100.0f)
			[
				// TODO: Check how to improve performance for this widget OnValueChanged.
				// Same functionality in LevelEditorToolBar.cpp seems to have better performance
				SNew(SNumericEntryBox<float>)
				.ToolTipText(Tooltip)
				.MinValue(0.0f)
				.MaxValue(1.0f)
				.MaxSliderValue(1.0f)
				.AllowSpin(true)
				.MaxFractionalDigits(1)
				.Font(FAppStyle::GetFontStyle(TEXT("MenuItem.Font")))
				.OnValueChanged_Static(&FLevelEditorActionCallbacks::SetActorSnapSetting)
				.Value_Lambda([]()
				{
					return FLevelEditorActionCallbacks::GetActorSnapSetting();
				})
			]
		]
		// clang-format on
	);

	return SnapDistance;
}

FToolMenuEntry CreateActorSnapCheckboxMenu()
{
	FNewToolMenuDelegate MakeMenuDelegate = FNewToolMenuDelegate::CreateLambda(
		[](UToolMenu* Submenu)
		{
			// Add "Actor snapping" widget.
			{
				FToolMenuSection& ActorSnappingSection =
					Submenu->FindOrAddSection("ActorSnapping", LOCTEXT("ActorSnappingLabel", "Actor Snapping"));
				ActorSnappingSection.AddEntry(CreateActorSnapDistanceEntry());
			}
		}
	);

	FToolUIAction CheckboxMenuAction;
	{
		CheckboxMenuAction.ExecuteAction = FToolMenuExecuteAction::CreateLambda(
			[](const FToolMenuContext& InContext)
			{
				if (ULevelEditorViewportSettings* Settings = GetMutableDefault<ULevelEditorViewportSettings>())
				{
					Settings->bEnableActorSnap = !Settings->bEnableActorSnap;
				}
			}
		);
		CheckboxMenuAction.GetActionCheckState = FToolMenuGetActionCheckState::CreateLambda(
			[](const FToolMenuContext& InContext)
			{
				return GetDefault<ULevelEditorViewportSettings>()->bEnableActorSnap ? ECheckBoxState::Checked
																					: ECheckBoxState::Unchecked;
			}
		);
	}

	FToolMenuEntry ActorSnapping = FToolMenuEntry::InitSubMenu(
		"ActorSnapping",
		LOCTEXT("ActorSnapLabel", "Actor"),
		FLevelEditorCommands::Get().EnableActorSnap->MakeTooltip()->GetTextTooltip(),
		MakeMenuDelegate,
		CheckboxMenuAction,
		EUserInterfaceActionType::ToggleButton
	);

	return ActorSnapping;
}

FToolMenuEntry CreateLocationSnapCheckboxMenu()
{
	const FName LocationSnapName = "LocationSnap";
	const FText LocationSnapLabel = LOCTEXT("LocationSnapLabel", "Location");

	if (!GEditor)
	{
		return FToolMenuEntry::InitMenuEntry(
			LocationSnapName, LocationSnapLabel, FText(), FSlateIcon(), UnrealEd::Private::DisabledAction()
		);
	}

	FNewToolMenuDelegate MakeMenuDelegate = FNewToolMenuDelegate::CreateLambda(
		[LocationSnapName](UToolMenu* InToolMenu)
		{
			UnrealEd::FLocationGridCheckboxListExecuteActionDelegate ExecuteDelegate =
				UnrealEd::FLocationGridCheckboxListExecuteActionDelegate::CreateUObject(GEditor, &UEditorEngine::SetGridSize);

			UnrealEd::FLocationGridCheckboxListIsCheckedDelegate IsCheckedDelegate =
				UnrealEd::FLocationGridCheckboxListIsCheckedDelegate::CreateLambda(
					[](int CurrGridSizeIndex)
					{
						const ULevelEditorViewportSettings* ViewportSettings = GetDefault<ULevelEditorViewportSettings>();
						return ViewportSettings->CurrentPosGridSize == CurrGridSizeIndex;
					}
				);

			const ULevelEditorViewportSettings* ViewportSettings = GetDefault<ULevelEditorViewportSettings>();
			TArray<float> GridSizes = ViewportSettings->bUsePowerOf2SnapSize ? ViewportSettings->Pow2GridSizes
																			 : ViewportSettings->DecimalGridSizes;

			InToolMenu->AddMenuEntry(
				LocationSnapName,
				FToolMenuEntry::InitWidget(
					LocationSnapName,
					UnrealEd::CreateLocationGridSnapMenu(
						ExecuteDelegate,
						IsCheckedDelegate,
						GridSizes,
						TAttribute<bool>::CreateLambda(
							[]()
							{
								return FLevelEditorActionCallbacks::LocationGridSnap_IsChecked();
							}
						)
					),
					FText()
				)
			);
		}
	);

	return UnrealEd::CreateCheckboxSubmenu(
		"GridSnapping",
		LocationSnapLabel,
		FEditorViewportCommands::Get().SurfaceSnapping->MakeTooltip()->GetTextTooltip(),
		FToolMenuExecuteAction::CreateLambda(
			[](const FToolMenuContext& InContext)
			{
				FLevelEditorActionCallbacks::LocationGridSnap_Clicked();
			}
		),
		FToolMenuCanExecuteAction(),
		FToolMenuGetActionCheckState::CreateLambda(
			[](const FToolMenuContext& InContext)
			{
				return FLevelEditorActionCallbacks::LocationGridSnap_IsChecked() ? ECheckBoxState::Checked
																				 : ECheckBoxState::Unchecked;
			}
		),
		MakeMenuDelegate
	);
}

FToolMenuEntry CreateRotationSnapCheckboxMenu()
{
	const FName RotationSnapName = "RotationSnap";
	const FText RotationSnapLabel = LOCTEXT("RotationSnapLabel", "Rotation");

	if (!GEditor)
	{
		return FToolMenuEntry::InitMenuEntry(
			RotationSnapName, RotationSnapLabel, FText(), FSlateIcon(), UnrealEd::Private::DisabledAction()
		);
	}

	FNewToolMenuDelegate MakeMenuDelegate = FNewToolMenuDelegate::CreateLambda(
		[RotationSnapName](UToolMenu* InToolMenu)
		{
			const ULevelEditorViewportSettings* ViewportSettings = GetDefault<ULevelEditorViewportSettings>();
			TArray<float> GridSizes = ViewportSettings->bUsePowerOf2SnapSize ? ViewportSettings->Pow2GridSizes
																			 : ViewportSettings->DecimalGridSizes;

			UnrealEd::FRotationGridCheckboxListExecuteActionDelegate ExecuteDelegate =
				UnrealEd::FRotationGridCheckboxListExecuteActionDelegate::CreateUObject(
					GEditor, &UEditorEngine::SetRotGridSize
				);

			UnrealEd::FRotationGridCheckboxListIsCheckedDelegate IsCheckedDelegate =
				UnrealEd::FRotationGridCheckboxListIsCheckedDelegate::CreateLambda(
					[](int CurrGridAngleIndex, ERotationGridMode InGridMode)
					{
						const ULevelEditorViewportSettings* ViewportSettings = GetDefault<ULevelEditorViewportSettings>();
						return ViewportSettings->CurrentRotGridSize == CurrGridAngleIndex
							&& ViewportSettings->CurrentRotGridMode == InGridMode;
					}
				);

			InToolMenu->AddMenuEntry(
				RotationSnapName,
				FToolMenuEntry::InitWidget(
					RotationSnapName,
					UnrealEd::CreateRotationGridSnapMenu(
						ExecuteDelegate,
						IsCheckedDelegate,
						TAttribute<bool>::CreateLambda(
							[]()
							{
								return FLevelEditorActionCallbacks::RotationGridSnap_IsChecked();
							}
						)
					),
					FText()
				)
			);
		}
	);

	return UnrealEd::CreateCheckboxSubmenu(
		"RotationSnapping",
		RotationSnapLabel,
		FEditorViewportCommands::Get().SurfaceSnapping->MakeTooltip()->GetTextTooltip(),
		FToolMenuExecuteAction::CreateLambda(
			[](const FToolMenuContext& InContext)
			{
				FLevelEditorActionCallbacks::RotationGridSnap_Clicked();
			}
		),
		FToolMenuCanExecuteAction(),
		FToolMenuGetActionCheckState::CreateLambda(
			[](const FToolMenuContext& InContext)
			{
				return FLevelEditorActionCallbacks::RotationGridSnap_IsChecked() ? ECheckBoxState::Checked
																				 : ECheckBoxState::Unchecked;
			}
		),
		MakeMenuDelegate
	);
}

FToolMenuEntry CreateScaleSnapCheckboxMenu()
{
	const FName ScaleSnapName = "ScaleSnap";
	const FText ScaleSnapLabel = LOCTEXT("ScaleSnapLabel", "Scale");

	if (!GEditor)
	{
		return FToolMenuEntry::InitMenuEntry(
			ScaleSnapName, ScaleSnapLabel, FText(), FSlateIcon(), UnrealEd::Private::DisabledAction()
		);
	}

	FNewToolMenuDelegate MakeMenuDelegate = FNewToolMenuDelegate::CreateLambda(
		[ScaleSnapName](UToolMenu* InToolMenu)
		{
			FCanExecuteAction CanExecuteScaleSnapping = FCanExecuteAction::CreateLambda(
				[]()
				{
					return FLevelEditorActionCallbacks::ScaleGridSnap_IsChecked();
				}
			);

			const ULevelEditorViewportSettings* ViewportSettings = GetDefault<ULevelEditorViewportSettings>();
			TArray<float> GridSizes = ViewportSettings->ScalingGridSizes;

			UnrealEd::FScaleGridCheckboxListExecuteActionDelegate ExecuteDelegate =
				UnrealEd::FScaleGridCheckboxListExecuteActionDelegate::CreateUObject(GEditor, &UEditorEngine::SetScaleGridSize);

			UnrealEd::FScaleGridCheckboxListIsCheckedDelegate IsCheckedDelegate =
				UnrealEd::FScaleGridCheckboxListIsCheckedDelegate::CreateLambda(
					[](int CurrGridSizeIndex)
					{
						const ULevelEditorViewportSettings* ViewportSettings = GetDefault<ULevelEditorViewportSettings>();
						return ViewportSettings->CurrentScalingGridSize == CurrGridSizeIndex;
					}
				);

			InToolMenu->AddMenuEntry(
				ScaleSnapName,
				FToolMenuEntry::InitWidget(
					ScaleSnapName,
					UnrealEd::CreateScaleGridSnapMenu(
						ExecuteDelegate,
						IsCheckedDelegate,
						GridSizes,
						TAttribute<bool>::CreateLambda(
							[]()
							{
								return FLevelEditorActionCallbacks::ScaleGridSnap_IsChecked();
							}
						),
						{},
						true,
						FUIAction(
							FExecuteAction::CreateLambda(
								[]()
								{
									ULevelEditorViewportSettings* Settings =
										GetMutableDefault<ULevelEditorViewportSettings>();
									Settings->PreserveNonUniformScale = !Settings->PreserveNonUniformScale;
								}
							),
							CanExecuteScaleSnapping,
							FIsActionChecked::CreateLambda(
								[]()
								{
									return GetDefault<ULevelEditorViewportSettings>()->PreserveNonUniformScale;
								}
							)
						)
					),
					FText()
				)
			);
		}
	);

	return UnrealEd::CreateCheckboxSubmenu(
		"ScaleSnapping",
		ScaleSnapLabel,
		FEditorViewportCommands::Get().SurfaceSnapping->MakeTooltip()->GetTextTooltip(),
		FToolMenuExecuteAction::CreateLambda(
			[](const FToolMenuContext& InContext)
			{
				FLevelEditorActionCallbacks::ScaleGridSnap_Clicked();
			}
		),
		FToolMenuCanExecuteAction(),
		FToolMenuGetActionCheckState::CreateLambda(
			[](const FToolMenuContext& InContext)
			{
				return FLevelEditorActionCallbacks::ScaleGridSnap_IsChecked() ? ECheckBoxState::Checked
																			  : ECheckBoxState::Unchecked;
			}
		),
		MakeMenuDelegate
	);
}

}

static FAutoConsoleVariableRef CVarToolMenusViewportToolbars(
	TEXT("ToolMenusViewportToolbars"),
	UE::UnrealEd::Private::CVarToolMenusViewportToolbarsValue,
	TEXT("Control whether the new ToolMenus-based viewport toolbars are enabled across the editor. Set to 0 (default) "
		 "to show only the old viewport toolbars. Set to 1 for side-by-side mode where both the old and new viewport "
		 "toolbars are shown. Set to 2 to show only the new viewport toolbars."),
	ECVF_Default
);

namespace UE::UnrealEd
{

bool ShowOldViewportToolbars()
{
	return Private::CVarToolMenusViewportToolbarsValue <= 1;
}

bool ShowNewViewportToolbars()
{
	return Private::CVarToolMenusViewportToolbarsValue >= 1;
}

FToolMenuEntry CreateViewportToolbarTransformsSection()
{
	return FToolMenuEntry::InitSubMenu(
		"Transforms",
		LOCTEXT("TransformsSubmenuLabel", "Transforms"),
		LOCTEXT("TransformsSubmenuTooltip", "Viewport-related transforms tools"),
		FNewToolMenuDelegate::CreateLambda(
			[](UToolMenu* Submenu) -> void
			{
				{
					FToolMenuSection& TransformToolsSection =
						Submenu->FindOrAddSection("TransformTools", LOCTEXT("TransformToolsLabel", "Transform Tools"));

					FToolMenuEntry SelectMode = FToolMenuEntry::InitMenuEntry(FEditorViewportCommands::Get().SelectMode);
					SelectMode.UserInterfaceActionType = EUserInterfaceActionType::RadioButton;
					SelectMode.SetShowInToolbarTopLevel(true);
					TransformToolsSection.AddEntry(SelectMode);

					FToolMenuEntry TranslateMode =
						FToolMenuEntry::InitMenuEntry(FEditorViewportCommands::Get().TranslateMode);
					TranslateMode.UserInterfaceActionType = EUserInterfaceActionType::RadioButton;
					TranslateMode.SetShowInToolbarTopLevel(true);
					TransformToolsSection.AddEntry(TranslateMode);

					FToolMenuEntry RotateMode = FToolMenuEntry::InitMenuEntry(FEditorViewportCommands::Get().RotateMode);
					RotateMode.UserInterfaceActionType = EUserInterfaceActionType::RadioButton;
					RotateMode.SetShowInToolbarTopLevel(true);
					TransformToolsSection.AddEntry(RotateMode);

					FToolMenuEntry ScaleMode = FToolMenuEntry::InitMenuEntry(FEditorViewportCommands::Get().ScaleMode);
					ScaleMode.UserInterfaceActionType = EUserInterfaceActionType::RadioButton;
					ScaleMode.SetShowInToolbarTopLevel(true);
					TransformToolsSection.AddEntry(ScaleMode);
				}

				{
					FToolMenuSection& SpacesSection = Submenu->FindOrAddSection("Spaces", LOCTEXT("SpacesLabel", "Spaces"));

					FToolMenuEntry WorldSpace =
						FToolMenuEntry::InitMenuEntry(FEditorViewportCommands::Get().RelativeCoordinateSystem_World);
					WorldSpace.UserInterfaceActionType = EUserInterfaceActionType::RadioButton;
					WorldSpace.SetShowInToolbarTopLevel(true);
					SpacesSection.AddEntry(WorldSpace);

					FToolMenuEntry LocalSpace =
						FToolMenuEntry::InitMenuEntry(FEditorViewportCommands::Get().RelativeCoordinateSystem_Local);
					LocalSpace.UserInterfaceActionType = EUserInterfaceActionType::RadioButton;
					LocalSpace.SetShowInToolbarTopLevel(true);
					SpacesSection.AddEntry(LocalSpace);
				}

				{
					FToolMenuSection& GizmoSection = Submenu->FindOrAddSection("Gizmo", LOCTEXT("GizmoLabel", "Gizmo"));

					GizmoSection.AddMenuEntry(
						FLevelEditorCommands::Get().ShowTransformWidget,
						LOCTEXT("ShowTransformGizmoLabel", "Show Transform Gizmo")
					);

					TSharedRef<SWidget> GizmoScaleWidget =
						// clang-format off
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.FillWidth(0.9f)
						[
							SNew(SSpinBox<int32>)
								.MinValue(-10)
								.MaxValue(150)
								.ToolTipText_Lambda(
									[]() -> FText
									{
										return FText::AsNumber(
											GetDefault<ULevelEditorViewportSettings>()->TransformWidgetSizeAdjustment
										);
									}
								)
								.Value_Lambda(
									[]() -> float
									{
										return GetDefault<ULevelEditorViewportSettings>()->TransformWidgetSizeAdjustment;
									}
								)
								.OnValueChanged_Lambda(
									[](float InValue)
									{
										ULevelEditorViewportSettings* ViewportSettings =
											GetMutableDefault<ULevelEditorViewportSettings>();
										ViewportSettings->TransformWidgetSizeAdjustment = InValue;
										ViewportSettings->PostEditChange();
									}
								)
						]
						+ SHorizontalBox::Slot()
						.FillWidth(0.1f);
					// clang-format on
					GizmoSection.AddEntry(FToolMenuEntry::InitWidget(
						"GizmoScale", GizmoScaleWidget, LOCTEXT("GizmoScaleLabel", "Gizmo Scale")
					));
				}
			}
		)
	);
}

FToolMenuEntry CreateViewportToolbarSelectionSection()
{
	return FToolMenuEntry::InitSubMenu(
		"Select",
		LOCTEXT("SelectonSubmenuLabel", "Select"),
		LOCTEXT("SelectionSubmenuTooltip", "Viewport-related selection tools"),
		FNewToolMenuDelegate::CreateLambda(
			[](UToolMenu* Submenu) -> void
			{
				{
					FToolMenuSection& UnnamedSection = Submenu->FindOrAddSection(NAME_None);

					UnnamedSection.AddMenuEntry(
						FGenericCommands::Get().SelectAll,
						FGenericCommands::Get().SelectAll->GetLabel(),
						FGenericCommands::Get().SelectAll->MakeTooltip()->GetTextTooltip(),
						FSlateIconFinder::FindIcon("FoliageEditMode.SelectAll")
					);

					UnnamedSection.AddMenuEntry(
						FLevelEditorCommands::Get().SelectNone,
						FLevelEditorCommands::Get().SelectNone->GetLabel(),
						FLevelEditorCommands::Get().SelectNone->MakeTooltip()->GetTextTooltip(),
						FSlateIconFinder::FindIcon("Cross")
					);

					UnnamedSection.AddMenuEntry(
						FLevelEditorCommands::Get().InvertSelection,
						FLevelEditorCommands::Get().InvertSelection->GetLabel(),
						FLevelEditorCommands::Get().InvertSelection->MakeTooltip()->GetTextTooltip(),
						FSlateIconFinder::FindIcon("FoliageEditMode.DeselectAll")
					);

					// Hierarchy based selection
					{
						UnnamedSection.AddSubMenu(
							"Hierarchy",
							LOCTEXT("HierarchyLabel", "Hierarchy"),
							LOCTEXT("HierarchyTooltip", "Hierarchy selection tools"),
							FNewToolMenuDelegate::CreateLambda(
								[](UToolMenu* HierarchyMenu)
								{
									FToolMenuSection& HierarchySection = HierarchyMenu->FindOrAddSection(
										"SelectAllHierarchy", LOCTEXT("SelectAllHierarchyLabel", "Hierarchy")
									);

									HierarchySection.AddMenuEntry(
										FLevelEditorCommands::Get().SelectImmediateChildren,
										LOCTEXT("HierarchySelectImmediateChildrenLabel", "Immediate Children")
									);

									HierarchySection.AddMenuEntry(
										FLevelEditorCommands::Get().SelectAllDescendants,
										LOCTEXT("HierarchySelectAllDescendantsLabel", "All Descendants")
									);
								}
							),
							false,
							FSlateIconFinder::FindIcon("BTEditor.SwitchToBehaviorTreeMode")
						);
					}

					UnnamedSection.AddSeparator("Advanced");

					UnnamedSection.AddMenuEntry(
						FLevelEditorCommands::Get().SelectAllActorsOfSameClass,
						LOCTEXT("AdvancedSelectAllActorsOfSameClassLabel", "All of Same Class"),
						FLevelEditorCommands::Get().SelectAllActorsOfSameClass->MakeTooltip()->GetTextTooltip(),
						FSlateIconFinder::FindIcon("PlacementBrowser.Icons.All")
					);
				}

				{
					FToolMenuSection& ByTypeSection =
						Submenu->FindOrAddSection("ByTypeSection", LOCTEXT("ByTypeSectionLabel", "By Type"));

					ByTypeSection.AddSubMenu(
						"BSP",
						LOCTEXT("BspLabel", "BSP"),
						LOCTEXT("BspTooltip", "BSP-related tools"),
						FNewToolMenuDelegate::CreateLambda(
							[](UToolMenu* BspMenu)
							{
								FToolMenuSection& SelectAllSection = BspMenu->FindOrAddSection(
									"SelectAllBSP", LOCTEXT("SelectAllBSPLabel", "Select All BSP")
								);

								SelectAllSection.AddMenuEntry(
									FLevelEditorCommands::Get().SelectAllAddditiveBrushes,
									LOCTEXT("BSPSelectAllAdditiveBrushesLabel", "Addditive Brushes")
								);

								SelectAllSection.AddMenuEntry(
									FLevelEditorCommands::Get().SelectAllSubtractiveBrushes,
									LOCTEXT("BSPSelectAllSubtractiveBrushesLabel", "Subtractive Brushes")
								);

								SelectAllSection.AddMenuEntry(
									FLevelEditorCommands::Get().SelectAllSurfaces,
									LOCTEXT("BSPSelectAllAllSurfacesLabel", "Surfaces")
								);
							}
						),
						false,
						FSlateIconFinder::FindIcon("ShowFlagsMenu.BSP")
					);

					ByTypeSection.AddSubMenu(
						"Emitters",
						LOCTEXT("EmittersLabel", "Emitters"),
						LOCTEXT("EmittersTooltip", "Emitters-related tools"),
						FNewToolMenuDelegate::CreateLambda(
							[](UToolMenu* EmittersMenu)
							{
								FToolMenuSection& SelectAllSection = EmittersMenu->FindOrAddSection(
									"SelectAllEmitters", LOCTEXT("SelectAllEmittersLabel", "Select All Emitters")
								);

								SelectAllSection.AddMenuEntry(
									FLevelEditorCommands::Get().SelectMatchingEmitter,
									LOCTEXT("EmittersSelectMatchingEmitterLabel", "Matching Emitters")
								);
							}
						),
						false,
						FSlateIconFinder::FindIcon("ClassIcon.Emitter")
					);

					ByTypeSection.AddSubMenu(
						"GeometryCollections",
						LOCTEXT("GeometryCollectionsLabel", "Geometry Collections"),
						LOCTEXT("GeometryCollectionsTooltip", "GeometryCollections-related tools"),
						FNewToolMenuDelegate::CreateLambda(
							[](UToolMenu* GeometryCollectionsMenu)
							{
								// This one will be filled by extensions from GeometryCollectionEditorPlugin
								// Hook is "SelectGeometryCollections"
								FToolMenuSection& SelectAllSection = GeometryCollectionsMenu->FindOrAddSection(
									"SelectGeometryCollections",
									LOCTEXT("SelectGeometryCollectionsLabel", "Geometry Collections")
								);
							}
						),
						false,
						FSlateIconFinder::FindIcon("ClassIcon.GeometryCollection")
					);

					ByTypeSection.AddSubMenu(
						"HLOD",
						LOCTEXT("HLODLabel", "HLOD"),
						LOCTEXT("HLODTooltip", "HLOD-related tools"),
						FNewToolMenuDelegate::CreateLambda(
							[](UToolMenu* HLODMenu)
							{
								FToolMenuSection& SelectAllSection = HLODMenu->FindOrAddSection(
									"SelectAllHLOD", LOCTEXT("SelectAllHLODLabel", "Select All HLOD")
								);

								SelectAllSection.AddMenuEntry(
									FLevelEditorCommands::Get().SelectOwningHierarchicalLODCluster,
									LOCTEXT("HLODSelectOwningHierarchicalLODClusterLabel", "Owning HLOD Cluster")
								);
							}
						),
						false,
						FSlateIconFinder::FindIcon("WorldPartition.ShowHLODActors")
					);

					ByTypeSection.AddSubMenu(
						"Lights",
						LOCTEXT("LightsLabel", "Lights"),
						LOCTEXT("LightsTooltip", "Lights-related tools"),
						FNewToolMenuDelegate::CreateLambda(
							[](UToolMenu* LightsMenu)
							{
								FToolMenuSection& SelectAllSection = LightsMenu->FindOrAddSection(
									"SelectAllLights", LOCTEXT("SelectAllLightsLabel", "Select All Lights")
								);

								SelectAllSection.AddMenuEntry(
									FLevelEditorCommands::Get().SelectAllLights,
									LOCTEXT("LightsSelectAllLightsLabel", "All Lights")
								);

								SelectAllSection.AddMenuEntry(
									FLevelEditorCommands::Get().SelectRelevantLights,
									LOCTEXT("LightsSelectRelevantLightsLabel", "Relevant Lights")
								);

								SelectAllSection.AddMenuEntry(
									FLevelEditorCommands::Get().SelectStationaryLightsExceedingOverlap,
									LOCTEXT("LightsSelectStationaryLightsExceedingOverlapLabel", "Stationary Lights Exceeding Overlap")
								);
							}
						),
						false,
						FSlateIconFinder::FindIcon("PlacementBrowser.Icons.Lights")
					);

					ByTypeSection.AddSubMenu(
						"Material",
						LOCTEXT("MaterialLabel", "Material"),
						LOCTEXT("MaterialTooltip", "Material-related tools"),
						FNewToolMenuDelegate::CreateLambda(
							[](UToolMenu* MaterialMenu)
							{
								FToolMenuSection& SelectAllSection = MaterialMenu->FindOrAddSection(
									"SelectAllMaterial", LOCTEXT("SelectAllMaterialLabel", "Select All Material")
								);

								SelectAllSection.AddMenuEntry(
									FLevelEditorCommands::Get().SelectAllWithSameMaterial,
									LOCTEXT("MaterialSelectAllWithSameMaterialLabel", "With Same Material")
								);
							}
						),
						false,
						FSlateIconFinder::FindIcon("ClassIcon.Material")
					);

					ByTypeSection.AddSubMenu(
						"SkeletalMeshes",
						LOCTEXT("SkeletalMeshesLabel", "Skeletal Meshes"),
						LOCTEXT("SkeletalMeshesTooltip", "SkeletalMeshes-related tools"),
						FNewToolMenuDelegate::CreateLambda(
							[](UToolMenu* SkeletalMeshesMenu)
							{
								FToolMenuSection& SelectAllSection = SkeletalMeshesMenu->FindOrAddSection(
									"SelectAllSkeletalMeshes",
									LOCTEXT("SelectAllSkeletalMeshesLabel", "Select All SkeletalMeshes")
								);

								SelectAllSection.AddMenuEntry(
									FLevelEditorCommands::Get().SelectSkeletalMeshesOfSameClass,
									LOCTEXT(
										"SkeletalMeshesSelectSkeletalMeshesOfSameClassLabel",
										"Using Selected Skeletal Meshes (Selected Actor Types)"
									)
								);

								SelectAllSection.AddMenuEntry(
									FLevelEditorCommands::Get().SelectSkeletalMeshesAllClasses,
									LOCTEXT(
										"SkeletalMeshesSelectSkeletalMeshesAllClassesLabel",
										"Using Selected Skeletal Meshes (All Actor Types)"
									)
								);
							}
						),
						false,
						FSlateIconFinder::FindIcon("SkeletonTree.Bone")
					);

					ByTypeSection.AddSubMenu(
						"StaticMeshes",
						LOCTEXT("StaticMeshesLabel", "Static Meshes"),
						LOCTEXT("StaticMeshesTooltip", "StaticMeshes-related tools"),
						FNewToolMenuDelegate::CreateLambda(
							[](UToolMenu* StaticMeshesMenu)
							{
								FToolMenuSection& SelectAllSection = StaticMeshesMenu->FindOrAddSection(
									"SelectAllStaticMeshes", LOCTEXT("SelectAllStaticMeshesLabel", "Select All StaticMeshes")
								);

								SelectAllSection.AddMenuEntry(
									FLevelEditorCommands::Get().SelectStaticMeshesOfSameClass,
									LOCTEXT("StaticMeshesSelectStaticMeshesOfSameClassLabel", "Matching Selected Class")
								);

								SelectAllSection.AddMenuEntry(
									FLevelEditorCommands::Get().SelectStaticMeshesAllClasses,
									LOCTEXT("StaticMeshesSelectStaticMeshesAllClassesLabel", "Matching All Classes")
								);
							}
						),
						false,
						FSlateIconFinder::FindIcon("ShowFlagsMenu.StaticMeshes")
					);
				}

				{
					FToolMenuSection& OptionsSection =
						Submenu->FindOrAddSection("Options", LOCTEXT("OptionsLabel", "Options"));

					OptionsSection.AddMenuEntry(
						FLevelEditorCommands::Get().AllowTranslucentSelection,
						LOCTEXT("OptionsAllowTranslucentSelectionLabel", "Translucent Objects")
					);

					OptionsSection.AddMenuEntry(
						FLevelEditorCommands::Get().AllowGroupSelection,
						LOCTEXT("OptionsAllowGroupSelectionLabel", "Select Groups")
					);

					OptionsSection.AddMenuEntry(
						FLevelEditorCommands::Get().StrictBoxSelect,
						LOCTEXT("OptionsStrictBoxSelectLabel", "Strict Marquee Selection")
					);

					OptionsSection.AddMenuEntry(
						FLevelEditorCommands::Get().TransparentBoxSelect,
						LOCTEXT("OptionsTransparentBoxSelectLabel", "Marquee Select Occluded")
					);
				}
			}
		)
	);
}

FToolMenuEntry CreateViewportToolbarSnappingSubmenu()
{
	return FToolMenuEntry::InitSubMenu(
		"Snapping",
		LOCTEXT("SnappingSubmenuLabel", "Snapping"),
		LOCTEXT("SnappingSubmenuTooltip", "Viewport-related snapping settings"),
		FNewToolMenuDelegate::CreateLambda(
			[](UToolMenu* Submenu) -> void
			{
				FToolMenuSection& SnappingSection =
					Submenu->FindOrAddSection("Snapping", LOCTEXT("SnappingLabel", "Snapping"));

				SnappingSection.AddEntry(Private::CreateSurfaceSnapCheckboxMenu());
				SnappingSection.AddEntry(Private::CreateLocationSnapCheckboxMenu()).SetShowInToolbarTopLevel(true);
				SnappingSection.AddEntry(Private::CreateRotationSnapCheckboxMenu());
				SnappingSection.AddEntry(Private::CreateScaleSnapCheckboxMenu());
				SnappingSection.AddEntry(Private::CreateActorSnapCheckboxMenu());

				FToolMenuEntry SocketSnapping =
					FToolMenuEntry::InitMenuEntry(FLevelEditorCommands::Get().ToggleSocketSnapping);
				SocketSnapping.UserInterfaceActionType = EUserInterfaceActionType::ToggleButton;
				SocketSnapping.Label = LOCTEXT("SocketSnapLabel", "Socket");
				SnappingSection.AddEntry(SocketSnapping);

				FToolMenuEntry VertexSnapping = FToolMenuEntry::InitMenuEntry(FLevelEditorCommands::Get().EnableVertexSnap);
				VertexSnapping.UserInterfaceActionType = EUserInterfaceActionType::ToggleButton;
				VertexSnapping.Label = LOCTEXT("VertexSnapLabel", "Vertex");
				SnappingSection.AddEntry(VertexSnapping);
			}
		)
	);
}

FText GetViewModesSubmenuLabel(TWeakPtr<SEditorViewport> InViewport)
{
	FText Label = LOCTEXT("ViewMenuTitle_Default", "View");
	if (TSharedPtr<SEditorViewport> PinnedViewport = InViewport.Pin())
	{
		const TSharedPtr<FEditorViewportClient> ViewportClient = PinnedViewport->GetViewportClient();
		check(ViewportClient.IsValid());
		const EViewModeIndex ViewMode = ViewportClient->GetViewMode();
		// If VMI_VisualizeBuffer, return its subcategory name
		if (ViewMode == VMI_VisualizeBuffer)
		{
			Label = ViewportClient->GetCurrentBufferVisualizationModeDisplayName();
		}
		else if (ViewMode == VMI_VisualizeNanite)
		{
			Label = ViewportClient->GetCurrentNaniteVisualizationModeDisplayName();
		}
		else if (ViewMode == VMI_VisualizeLumen)
		{
			Label = ViewportClient->GetCurrentLumenVisualizationModeDisplayName();
		}
		else if (ViewMode == VMI_VisualizeSubstrate)
		{
			Label = ViewportClient->GetCurrentSubstrateVisualizationModeDisplayName();
		}
		else if (ViewMode == VMI_VisualizeGroom)
		{
			Label = ViewportClient->GetCurrentGroomVisualizationModeDisplayName();
		}
		else if (ViewMode == VMI_VisualizeVirtualShadowMap)
		{
			Label = ViewportClient->GetCurrentVirtualShadowMapVisualizationModeDisplayName();
		}
		else if (ViewMode == VMI_VisualizeActorColoration)
		{
			Label = ViewportClient->GetCurrentActorColorationVisualizationModeDisplayName();
		}
		else if (ViewMode == VMI_VisualizeGPUSkinCache)
		{
			Label = ViewportClient->GetCurrentGPUSkinCacheVisualizationModeDisplayName();
		}
		// For any other category, return its own name
		else
		{
			Label = UViewModeUtils::GetViewModeDisplayName(ViewMode);
		}
	}

	return Label;
}

void PopulateViewModesMenu(
	UToolMenu* InMenu, TSharedRef<SEditorViewport> InViewport, IsViewModeSupportedDelegate InIsViewModeSupported)
{
	const FEditorViewportCommands& BaseViewportActions = FEditorViewportCommands::Get();

	// View modes
	{
		FToolMenuSection& Section = InMenu->AddSection("ViewMode", LOCTEXT("ViewModeHeader", "View Mode"));
		{
			Section.AddMenuEntry(BaseViewportActions.LitMode, UViewModeUtils::GetViewModeDisplayName(VMI_Lit));
			Section.AddMenuEntry(BaseViewportActions.UnlitMode, UViewModeUtils::GetViewModeDisplayName(VMI_Unlit));
			Section.AddMenuEntry(
				BaseViewportActions.WireframeMode, UViewModeUtils::GetViewModeDisplayName(VMI_BrushWireframe));
			Section.AddMenuEntry(
				BaseViewportActions.LitWireframeMode, UViewModeUtils::GetViewModeDisplayName(VMI_Lit_Wireframe));
			Section.AddMenuEntry(
				BaseViewportActions.DetailLightingMode, UViewModeUtils::GetViewModeDisplayName(VMI_Lit_DetailLighting));
			Section.AddMenuEntry(
				BaseViewportActions.LightingOnlyMode, UViewModeUtils::GetViewModeDisplayName(VMI_LightingOnly));
			Section.AddMenuEntry(BaseViewportActions.ReflectionOverrideMode,
				UViewModeUtils::GetViewModeDisplayName(VMI_ReflectionOverride));
			Section.AddMenuEntry(
				BaseViewportActions.CollisionPawn, UViewModeUtils::GetViewModeDisplayName(VMI_CollisionPawn));
			Section.AddMenuEntry(BaseViewportActions.CollisionVisibility,
				UViewModeUtils::GetViewModeDisplayName(VMI_CollisionVisibility));
		}

		if (IsRayTracingEnabled())
		{
			static auto PathTracingCvar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.PathTracing"));
			const bool bPathTracingSupported = FDataDrivenShaderPlatformInfo::GetSupportsPathTracing(GMaxRHIShaderPlatform);
			const bool bPathTracingEnabled = PathTracingCvar && PathTracingCvar->GetValueOnAnyThread() != 0;
			if (bPathTracingSupported && bPathTracingEnabled)
			{
				Section.AddMenuEntry(
					BaseViewportActions.PathTracingMode, UViewModeUtils::GetViewModeDisplayName(VMI_PathTracing));
			}
		}

		// Optimization
		{
			struct Local
			{
				static void BuildOptimizationMenu(UToolMenu* Menu, IsViewModeSupportedDelegate IsViewModeSupported)
				{
					const FEditorViewportCommands& BaseViewportCommands = FEditorViewportCommands::Get();

					UWorld* World = GWorld;
					const ERHIFeatureLevel::Type FeatureLevel = (IsInGameThread() && World)
																  ? (ERHIFeatureLevel::Type)World->GetFeatureLevel()
																  : GMaxRHIFeatureLevel;

					{
						FToolMenuSection& Section = Menu->AddSection(
							"OptimizationViewmodes", LOCTEXT("OptimizationSubMenuHeader", "Optimization Viewmodes"));
						if (FeatureLevel >= ERHIFeatureLevel::SM5)
						{
							Section.AddMenuEntry(BaseViewportCommands.LightComplexityMode,
								UViewModeUtils::GetViewModeDisplayName(VMI_LightComplexity));
							if (IsStaticLightingAllowed())
							{
								Section.AddMenuEntry(BaseViewportCommands.LightmapDensityMode,
									UViewModeUtils::GetViewModeDisplayName(VMI_LightmapDensity));
							}
							Section.AddMenuEntry(BaseViewportCommands.StationaryLightOverlapMode,
								UViewModeUtils::GetViewModeDisplayName(VMI_StationaryLightOverlap));
						}

						Section.AddMenuEntry(BaseViewportCommands.ShaderComplexityMode,
							UViewModeUtils::GetViewModeDisplayName(VMI_ShaderComplexity));

						if (AllowDebugViewShaderMode(
								DVSM_ShaderComplexityContainedQuadOverhead, GMaxRHIShaderPlatform, FeatureLevel))
						{
							Section.AddMenuEntry(BaseViewportCommands.ShaderComplexityWithQuadOverdrawMode,
								UViewModeUtils::GetViewModeDisplayName(VMI_ShaderComplexityWithQuadOverdraw));
						}
						if (AllowDebugViewShaderMode(DVSM_QuadComplexity, GMaxRHIShaderPlatform, FeatureLevel))
						{
							Section.AddMenuEntry(BaseViewportCommands.QuadOverdrawMode,
								UViewModeUtils::GetViewModeDisplayName(VMI_QuadOverdraw));
						}
						if (AllowDebugViewShaderMode(DVSM_LWCComplexity, GMaxRHIShaderPlatform, FeatureLevel))
						{
							Section.AddMenuEntry(BaseViewportCommands.VisualizeLWCComplexity,
								UViewModeUtils::GetViewModeDisplayName(VMI_LWCComplexity), TAttribute<FText>(),
								FSlateIcon(FAppStyle::GetAppStyleSetName(), "EditorViewport.LWCComplexityMode"));
						}
					}

					{
						FToolMenuSection& Section = Menu->AddSection(
							"TextureStreaming", LOCTEXT("TextureStreamingHeader", "Texture Streaming Accuracy"));

						if (AllowDebugViewShaderMode(DVSM_PrimitiveDistanceAccuracy, GMaxRHIShaderPlatform, FeatureLevel)
							&& (!IsViewModeSupported.IsBound()
								|| IsViewModeSupported.Execute(VMI_PrimitiveDistanceAccuracy)))
						{
							Section.AddMenuEntry(BaseViewportCommands.TexStreamAccPrimitiveDistanceMode,
								UViewModeUtils::GetViewModeDisplayName(VMI_PrimitiveDistanceAccuracy));
						}
						if (AllowDebugViewShaderMode(DVSM_MeshUVDensityAccuracy, GMaxRHIShaderPlatform, FeatureLevel)
							&& (!IsViewModeSupported.IsBound() || IsViewModeSupported.Execute(VMI_MeshUVDensityAccuracy)))
						{
							Section.AddMenuEntry(BaseViewportCommands.TexStreamAccMeshUVDensityMode,
								UViewModeUtils::GetViewModeDisplayName(VMI_MeshUVDensityAccuracy));
						}
						// TexCoordScale accuracy viewmode requires shaders that are only built in the
						// TextureStreamingBuild, which requires the new metrics to be enabled.
						if (AllowDebugViewShaderMode(DVSM_MaterialTextureScaleAccuracy, GMaxRHIShaderPlatform, FeatureLevel)
							&& CVarStreamingUseNewMetrics.GetValueOnAnyThread() != 0
							&& (!IsViewModeSupported.IsBound()
								|| IsViewModeSupported.Execute(VMI_MaterialTextureScaleAccuracy)))
						{
							Section.AddMenuEntry(BaseViewportCommands.TexStreamAccMaterialTextureScaleMode,
								UViewModeUtils::GetViewModeDisplayName(VMI_MaterialTextureScaleAccuracy));
						}
						if (AllowDebugViewShaderMode(DVSM_RequiredTextureResolution, GMaxRHIShaderPlatform, FeatureLevel)
							&& (!IsViewModeSupported.IsBound()
								|| IsViewModeSupported.Execute(VMI_RequiredTextureResolution)))
						{
							Section.AddMenuEntry(BaseViewportCommands.RequiredTextureResolutionMode,
								UViewModeUtils::GetViewModeDisplayName(VMI_RequiredTextureResolution));
						}
						if (AllowDebugViewShaderMode(DVSM_RequiredTextureResolution, GMaxRHIShaderPlatform, FeatureLevel)
							&& (!IsViewModeSupported.IsBound()
								|| IsViewModeSupported.Execute(VMI_VirtualTexturePendingMips)))
						{
							Section.AddMenuEntry(BaseViewportCommands.VirtualTexturePendingMipsMode,
								UViewModeUtils::GetViewModeDisplayName(VMI_VirtualTexturePendingMips));
						}
					}
				}
			};

			Section.AddSubMenu("OptimizationSubMenu", LOCTEXT("OptimizationSubMenu", "Optimization Viewmodes"),
				LOCTEXT("Optimization_ToolTip", "Select optimization visualizer"),
				FNewToolMenuDelegate::CreateStatic(&Local::BuildOptimizationMenu, InIsViewModeSupported),
				FUIAction(FExecuteAction(), FCanExecuteAction(),
					FIsActionChecked::CreateLambda([Viewport = InViewport.ToWeakPtr()]() {
						const TSharedRef<SEditorViewport> ViewportRef = Viewport.Pin().ToSharedRef();
						const TSharedPtr<FEditorViewportClient> ViewportClient = ViewportRef->GetViewportClient();
						check(ViewportClient.IsValid());
						const EViewModeIndex ViewMode = ViewportClient->GetViewMode();
						return (
							// Texture Streaming Accuracy
							ViewMode == VMI_LightComplexity || ViewMode == VMI_LightmapDensity
							|| ViewMode == VMI_StationaryLightOverlap || ViewMode == VMI_ShaderComplexity
							|| ViewMode == VMI_ShaderComplexityWithQuadOverdraw
							|| ViewMode == VMI_QuadOverdraw
							// Texture Streaming Accuracy
							|| ViewMode == VMI_PrimitiveDistanceAccuracy || ViewMode == VMI_MeshUVDensityAccuracy
							|| ViewMode == VMI_MaterialTextureScaleAccuracy || ViewMode == VMI_RequiredTextureResolution
							|| ViewMode == VMI_VirtualTexturePendingMips);
					})),
				EUserInterfaceActionType::RadioButton,
				/* bInOpenSubMenuOnClick = */ false,
				FSlateIcon(FAppStyle::GetAppStyleSetName(), "EditorViewport.QuadOverdrawMode"));
		}

		if (IsRayTracingEnabled())
		{
			struct Local
			{
				static void BuildRayTracingDebugMenu(FMenuBuilder& Menu) //, TWeakPtr<SViewportToolBar> InParentToolBar)
				{
					const FRayTracingDebugVisualizationMenuCommands& RtDebugCommands =
						FRayTracingDebugVisualizationMenuCommands::Get();
					RtDebugCommands.BuildVisualisationSubMenu(Menu);
				}
			};

			Section.AddSubMenu("RayTracingDebugSubMenu", LOCTEXT("RayTracingDebugSubMenu", "Ray Tracing Debug"),
				LOCTEXT("RayTracing_ToolTip", "Select ray tracing buffer visualization view modes"),
				FNewMenuDelegate::CreateStatic(&Local::BuildRayTracingDebugMenu), //, ParentToolBar)
				false, FSlateIcon(FAppStyle::GetAppStyleSetName(), "EditorViewport.RayTracingDebugMode"));
		}

		{
			struct Local
			{
				static void BuildLODMenu(UToolMenu* Menu)
				{
					{
						FToolMenuSection& Section = Menu->AddSection(
							"LevelViewportLODColoration", LOCTEXT("LODModesHeader", "Level of Detail Coloration"));
						Section.AddMenuEntry(FEditorViewportCommands::Get().LODColorationMode,
							UViewModeUtils::GetViewModeDisplayName(VMI_LODColoration));
						Section.AddMenuEntry(FEditorViewportCommands::Get().HLODColorationMode,
							UViewModeUtils::GetViewModeDisplayName(VMI_HLODColoration));
					}
				}
			};

			Section.AddSubMenu("VisualizeGroupedLOD",
				LOCTEXT("VisualizeGroupedLODDisplayName", "Level of Detail Coloration"),
				LOCTEXT("GroupedLODMenu_ToolTip", "Select a mode for LOD Coloration"),
				FNewToolMenuDelegate::CreateStatic(&Local::BuildLODMenu),
				FUIAction(FExecuteAction(), FCanExecuteAction(),
					FIsActionChecked::CreateLambda([WeakViewport = InViewport.ToWeakPtr()]() {
						const TSharedRef<SEditorViewport> ViewportRef = WeakViewport.Pin().ToSharedRef();
						const TSharedPtr<FEditorViewportClient> ViewportClient = ViewportRef->GetViewportClient();
						check(ViewportClient.IsValid());
						const EViewModeIndex ViewMode = ViewportClient->GetViewMode();
						return (ViewMode == VMI_LODColoration || ViewMode == VMI_HLODColoration);
					})),
				EUserInterfaceActionType::RadioButton,
				/* bInOpenSubMenuOnClick = */ false,
				FSlateIcon(FAppStyle::GetAppStyleSetName(), "EditorViewport.GroupLODColorationMode"));
		}

		if (GEnableGPUSkinCache)
		{
			Section.AddSubMenu("VisualizeGPUSkinCacheViewMode",
				LOCTEXT("VisualizeGPUSkinCacheViewModeDisplayName", "GPU Skin Cache"),
				LOCTEXT("GPUSkinCacheVisualizationMenu_ToolTip", "Select a mode for GPU Skin Cache visualization."),
				FNewMenuDelegate::CreateStatic(&FGPUSkinCacheVisualizationMenuCommands::BuildVisualisationSubMenu),
				FUIAction(FExecuteAction(), FCanExecuteAction(),
					FIsActionChecked::CreateLambda([WeakViewport = InViewport.ToWeakPtr()]() {
						const TSharedRef<SEditorViewport> ViewportRef = WeakViewport.Pin().ToSharedRef();
						const TSharedPtr<FEditorViewportClient> ViewportClient = ViewportRef->GetViewportClient();
						check(ViewportClient.IsValid());
						return ViewportClient->IsViewModeEnabled(VMI_VisualizeGPUSkinCache);
					})),
				EUserInterfaceActionType::RadioButton,
				/* bInOpenSubMenuOnClick = */ false,
				FSlateIcon(FAppStyle::GetAppStyleSetName(), "EditorViewport.VisualizeGPUSkinCacheMode"));
		}
	}

	// Auto Exposure
	{
		const FEditorViewportCommands& BaseViewportCommands = FEditorViewportCommands::Get();

		TSharedRef<SWidget> FixedEV100Menu = InViewport->BuildFixedEV100Menu();
		TSharedPtr<FEditorViewportClient> EditorViewPostClient = InViewport->GetViewportClient();
		const bool bIsLevelEditor = EditorViewPostClient.IsValid() && EditorViewPostClient->IsLevelEditorClient();

		FToolMenuSection& Section = InMenu->AddSection("Exposure", LOCTEXT("ExposureHeader", "Exposure"));
		Section.AddMenuEntry(
			bIsLevelEditor ? BaseViewportCommands.ToggleInGameExposure : BaseViewportCommands.ToggleAutoExposure);
		Section.AddEntry(FToolMenuEntry::InitWidget("FixedEV100", FixedEV100Menu, LOCTEXT("FixedEV100", "EV100")));
	}

	// Wireframe Opacity
	{
		TSharedRef<SWidget> WireOpacityMenu = InViewport->BuildWireframeMenu();
		FToolMenuSection& Section = InMenu->AddSection("Wireframe", LOCTEXT("WireframeHeader", "Wireframe"));
		Section.AddEntry(
			FToolMenuEntry::InitWidget("WireframeOpacity", WireOpacityMenu, LOCTEXT("WireframeOpacity", "Opacity")));
	}
}

FToolMenuEntry CreateViewportToolbarViewModesSubmenu()
{
	// This has to be a dynamic entry for the ViewModes submenu's label to be able to access the context.
	return FToolMenuEntry::InitDynamicEntry(
		"DynamicViewModes",
		FNewToolMenuSectionDelegate::CreateLambda(
			[](FToolMenuSection& InDynamicSection) -> void
			{
				// Base the label on the current view mode.
				TAttribute<FText> LabelAttribute = UE::UnrealEd::GetViewModesSubmenuLabel(nullptr);
				if (UUnrealEdViewportToolbarContext* const Context =
						InDynamicSection.FindContext<UUnrealEdViewportToolbarContext>())
				{
					LabelAttribute = TAttribute<FText>::CreateLambda(
						[WeakViewport = Context->Viewport]()
						{
							return UE::UnrealEd::GetViewModesSubmenuLabel(WeakViewport);
						}
					);
				}

				InDynamicSection.AddSubMenu(
					"ViewModes",
					LabelAttribute,
					LOCTEXT("ViewModesSubmenuTooltip", "View mode settings for the current viewport."),
					FNewToolMenuDelegate::CreateLambda(
						[](UToolMenu* Submenu) -> void
						{
							UUnrealEdViewportToolbarContext* const Context =
								Submenu->FindContext<UUnrealEdViewportToolbarContext>();
							if (!Context)
							{
								return;
							}

							if (const TSharedPtr<SEditorViewport> Viewport = Context->Viewport.Pin())
							{
								PopulateViewModesMenu(Submenu, Viewport.ToSharedRef(), Context->IsViewModeSupported);
							}
						}
					)
				);
			}
		)
	);
}

TSharedRef<SWidget> BuildRotationGridCheckBoxList(
	FName InExtentionHook,
	const FText& InHeading,
	const TArray<float>& InGridSizes,
	ERotationGridMode InGridMode,
	const FRotationGridCheckboxListExecuteActionDelegate& InExecuteAction,
	const FRotationGridCheckboxListIsCheckedDelegate& InIsActionChecked,
	const TSharedPtr<FUICommandList>& InCommandList
)
{
	constexpr bool bShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder RotationGridMenuBuilder(bShouldCloseWindowAfterMenuSelection, InCommandList);

	RotationGridMenuBuilder.BeginSection(InExtentionHook, InHeading);
	for (int32 CurrGridAngleIndex = 0; CurrGridAngleIndex < InGridSizes.Num(); ++CurrGridAngleIndex)
	{
		const float CurrGridAngle = InGridSizes[CurrGridAngleIndex];

		FText MenuName =
			FText::Format(LOCTEXT("RotationGridAngle", "{0}\u00b0"), FText::AsNumber(CurrGridAngle)); /*degree symbol*/
		FText ToolTipText = FText::Format(
			LOCTEXT("RotationGridAngle_ToolTip", "Sets rotation grid angle to {0}"), MenuName
		); /*degree symbol*/

		RotationGridMenuBuilder.AddMenuEntry(
			MenuName,
			ToolTipText,
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda(
					[CurrGridAngleIndex, InGridMode, InExecuteAction]()
					{
						InExecuteAction.Execute(CurrGridAngleIndex, InGridMode);
					}
				),
				FCanExecuteAction(),
				FIsActionChecked::CreateLambda(
					[CurrGridAngleIndex, InGridMode, InIsActionChecked]()
					{
						return InIsActionChecked.Execute(CurrGridAngleIndex, InGridMode);
					}
				)
			),
			NAME_None,
			EUserInterfaceActionType::RadioButton
		);
	}
	RotationGridMenuBuilder.EndSection();

	return RotationGridMenuBuilder.MakeWidget();
}

TSharedRef<SWidget> CreateRotationGridSnapMenu(
	const FRotationGridCheckboxListExecuteActionDelegate& InExecuteDelegate,
	const FRotationGridCheckboxListIsCheckedDelegate& InIsCheckedDelegate,
	const TAttribute<bool>& InIsEnabledDelegate,
	const TSharedPtr<FUICommandList>& InCommandList
)
{
	const ULevelEditorViewportSettings* ViewportSettings = GetDefault<ULevelEditorViewportSettings>();
	TArray<float> GridSizes = ViewportSettings->bUsePowerOf2SnapSize ? ViewportSettings->Pow2GridSizes
																	 : ViewportSettings->DecimalGridSizes;

	// clang-format off
	return SNew(SUniformGridPanel)
		.IsEnabled(InIsEnabledDelegate)
		+ SUniformGridPanel::Slot(0, 0)
		[
			UnrealEd::BuildRotationGridCheckBoxList("Common",LOCTEXT("RotationCommonText", "Rotation Increment")
				, ViewportSettings->CommonRotGridSizes, GridMode_Common,
				InExecuteDelegate,
				InIsCheckedDelegate,
				InCommandList
			)
		]
		+ SUniformGridPanel::Slot(1, 0)
		[
			UnrealEd::BuildRotationGridCheckBoxList("Div360",LOCTEXT("RotationDivisions360DegreesText", "Divisions of 360\u00b0")
				, ViewportSettings->DivisionsOf360RotGridSizes, GridMode_DivisionsOf360,
				InExecuteDelegate,
				InIsCheckedDelegate,
				InCommandList
			)
		];
	// clang-format on
}

TSharedRef<SWidget> CreateLocationGridSnapMenu(
	const FLocationGridCheckboxListExecuteActionDelegate& InExecuteDelegate,
	const FLocationGridCheckboxListIsCheckedDelegate& InIsCheckedDelegate,
	const TArray<float>& InGridSizes,
	const TAttribute<bool>& InIsEnabledDelegate,
	const TSharedPtr<FUICommandList>& InCommandList
)
{
	constexpr bool bShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder LocationGridMenuBuilder(bShouldCloseWindowAfterMenuSelection, InCommandList);

	LocationGridMenuBuilder.BeginSection("Snap", LOCTEXT("LocationSnapText", "Snap Sizes"));
	for (int32 CurrGridSizeIndex = 0; CurrGridSizeIndex < InGridSizes.Num(); ++CurrGridSizeIndex)
	{
		const float CurGridSize = InGridSizes[CurrGridSizeIndex];

		LocationGridMenuBuilder.AddMenuEntry(
			FText::AsNumber(CurGridSize),
			FText::Format(LOCTEXT("LocationGridSize_ToolTip", "Sets grid size to {0}"), FText::AsNumber(CurGridSize)),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda(
					[CurrGridSizeIndex, InExecuteDelegate]()
					{
						InExecuteDelegate.Execute(CurrGridSizeIndex);
					}
				),
				FCanExecuteAction::CreateLambda(
					[InIsEnabledDelegate]()
					{
						return InIsEnabledDelegate.Get();
					}
				),
				FIsActionChecked::CreateLambda(
					[CurrGridSizeIndex, InIsCheckedDelegate]()
					{
						return InIsCheckedDelegate.Execute(CurrGridSizeIndex);
					}
				)
			),
			NAME_None,
			EUserInterfaceActionType::RadioButton
		);
	}
	LocationGridMenuBuilder.EndSection();

	return LocationGridMenuBuilder.MakeWidget();
}

TSharedRef<SWidget> CreateScaleGridSnapMenu(
	const FScaleGridCheckboxListExecuteActionDelegate& InExecuteDelegate,
	const FScaleGridCheckboxListIsCheckedDelegate& InIsCheckedDelegate,
	const TArray<float>& InGridSizes,
	const TAttribute<bool>& InIsEnabledDelegate,
	const TSharedPtr<FUICommandList>& InCommandList,
	const TAttribute<bool>& ShowPreserveNonUniformScaleOption,
	const FUIAction& PreserveNonUniformScaleUIAction
)
{
	FNumberFormattingOptions NumberFormattingOptions;
	NumberFormattingOptions.MaximumFractionalDigits = 5;

	constexpr bool bShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder ScaleGridMenuBuilder(bShouldCloseWindowAfterMenuSelection, InCommandList);

	ScaleGridMenuBuilder.BeginSection("ScaleSnapOptions", LOCTEXT("ScaleSnapOptions", "Scale Snap"));

	for (int32 CurrGridAmountIndex = 0; CurrGridAmountIndex < InGridSizes.Num(); ++CurrGridAmountIndex)
	{
		const float CurGridAmount = InGridSizes[CurrGridAmountIndex];

		FText MenuText;
		FText ToolTipText;

		if (GEditor->UsePercentageBasedScaling())
		{
			MenuText = FText::AsPercent(CurGridAmount / 100.0f, &NumberFormattingOptions);
			ToolTipText = FText::Format(LOCTEXT("ScaleGridAmountOld_ToolTip", "Snaps scale values to {0}"), MenuText);
		}
		else
		{
			MenuText = FText::AsNumber(CurGridAmount, &NumberFormattingOptions);
			ToolTipText =
				FText::Format(LOCTEXT("ScaleGridAmount_ToolTip", "Snaps scale values to increments of {0}"), MenuText);
		}

		ScaleGridMenuBuilder.AddMenuEntry(
			MenuText,
			ToolTipText,
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda(
					[CurrGridAmountIndex, InExecuteDelegate]()
					{
						InExecuteDelegate.Execute(CurrGridAmountIndex);
					}
				),
				FCanExecuteAction::CreateLambda(
					[InIsEnabledDelegate]()
					{
						return InIsEnabledDelegate.Get();
					}
				),
				FIsActionChecked::CreateLambda(
					[CurrGridAmountIndex, InIsCheckedDelegate]()
					{
						return InIsCheckedDelegate.Execute(CurrGridAmountIndex);
					}
				)
			),
			NAME_None,
			EUserInterfaceActionType::RadioButton
		);
	}
	ScaleGridMenuBuilder.EndSection();

	if (!GEditor->UsePercentageBasedScaling() && ShowPreserveNonUniformScaleOption.Get())
	{
		ScaleGridMenuBuilder.BeginSection("ScaleGeneralOptions", LOCTEXT("ScaleOptions", "Scaling Options"));

		ScaleGridMenuBuilder.AddMenuEntry(
			LOCTEXT("ScaleGridPreserveNonUniformScale", "Preserve Non-Uniform Scale"),
			LOCTEXT(
				"ScaleGridPreserveNonUniformScale_ToolTip", "When this option is checked, scaling objects that have a non-uniform scale will preserve the ratios between each axis, snapping the axis with the largest value."
			),
			FSlateIcon(),
			PreserveNonUniformScaleUIAction,
			NAME_None,
			EUserInterfaceActionType::ToggleButton
		);

		ScaleGridMenuBuilder.EndSection();
	}

	return ScaleGridMenuBuilder.MakeWidget();
}

FToolMenuEntry CreateCheckboxSubmenu(
	const FName InName,
	const TAttribute<FText>& InLabel,
	const TAttribute<FText>& InToolTip,
	const FToolMenuExecuteAction& InCheckboxExecuteAction,
	const FToolMenuCanExecuteAction& InCheckboxCanExecuteAction,
	const FToolMenuGetActionCheckState& InCheckboxActionCheckState,
	const FNewToolMenuChoice& InMakeMenu
)
{
	FToolUIAction CheckboxMenuAction;
	{
		CheckboxMenuAction.ExecuteAction = InCheckboxExecuteAction;
		CheckboxMenuAction.CanExecuteAction = InCheckboxCanExecuteAction;
		CheckboxMenuAction.GetActionCheckState = InCheckboxActionCheckState;
	}

	FToolMenuEntry CheckBoxSubmenu = FToolMenuEntry::InitSubMenu(
		InName, InLabel, InToolTip, InMakeMenu, CheckboxMenuAction, EUserInterfaceActionType::ToggleButton
	);

	return CheckBoxSubmenu;
}

FToolMenuEntry CreateNumericEntry(
	const FName InName,
	const FText& InLabel,
	const FText& InTooltip,
	const FCanExecuteAction& InCanExecuteAction,
	const FNumericEntryExecuteActionDelegate& InOnValueChanged,
	const TAttribute<float>& InGetValue,
	float InMinValue,
	float InMaxValue,
	int32 InMaxFractionalDigits
)
{
	const FMargin WidgetsMargin(2.0f, 0.0f, 3.0f, 0.0f);

	FToolMenuEntry NumericEntry = FToolMenuEntry::InitMenuEntry(
		InName,
		FUIAction(FExecuteAction(), InCanExecuteAction),
		// clang-format off
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Left)
		.Padding(WidgetsMargin)
		.AutoWidth()
		[
			SNew(STextBlock)
			.Text(InLabel)
		]
		+ SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Right)
		.Padding(FMargin(6.0f, 0))
		.FillContentWidth(1.0)
		[
			SNew(SBox)
			.Padding(WidgetsMargin)
			.MinDesiredWidth(80.0f)
			[
				SNew(SNumericEntryBox<float>)
				.ToolTipText(InTooltip)
				.MinValue(InMinValue)
				.MaxValue(InMaxValue)
				.MaxSliderValue(InMaxValue)
				.AllowSpin(true)
				.MaxFractionalDigits(InMaxFractionalDigits)
				.Font(FAppStyle::GetFontStyle(TEXT("MenuItem.Font")))
				.OnValueChanged_Lambda([InOnValueChanged](float InValue){ InOnValueChanged.Execute(InValue); })
				.Value_Lambda([InGetValue](){ return InGetValue.Get(); })
			]
		]
		// clang-format on
	);

	return NumericEntry;
}

TSharedRef<SWidget> CreateCameraMenuWidget(const TSharedRef<SEditorViewport>& InViewport)
{
	constexpr bool bInShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder CameraMenuBuilder(bInShouldCloseWindowAfterMenuSelection, InViewport->GetCommandList());

	// Camera types
	CameraMenuBuilder.AddMenuEntry(FEditorViewportCommands::Get().Perspective);

	CameraMenuBuilder.BeginSection("LevelViewportCameraType_Ortho", LOCTEXT("CameraTypeHeader_Ortho", "Orthographic"));
	CameraMenuBuilder.AddMenuEntry(FEditorViewportCommands::Get().Top);
	CameraMenuBuilder.AddMenuEntry(FEditorViewportCommands::Get().Bottom);
	CameraMenuBuilder.AddMenuEntry(FEditorViewportCommands::Get().Left);
	CameraMenuBuilder.AddMenuEntry(FEditorViewportCommands::Get().Right);
	CameraMenuBuilder.AddMenuEntry(FEditorViewportCommands::Get().Front);
	CameraMenuBuilder.AddMenuEntry(FEditorViewportCommands::Get().Back);
	CameraMenuBuilder.EndSection();

	return CameraMenuBuilder.MakeWidget();
}

TSharedRef<SWidget> CreateFOVMenuWidget(const TSharedRef<SEditorViewport>& InViewport)
{
	constexpr float FOVMin = 5.0f;
	constexpr float FOVMax = 170.0f;

	TSharedPtr<FEditorViewportClient> ViewportClient = InViewport->GetViewportClient();

	return
		// clang-format off
		SNew(SBox)
		.HAlign(HAlign_Right)
		[
			SNew(SBox)
			.Padding(FMargin(4.0f, 0.0f, 0.0f, 0.0f))
			.WidthOverride(100.0f)
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::Get().GetBrush("Menu.WidgetBorder"))
				.Padding(FMargin(1.0f))
				[
					SNew(SSpinBox<float>)
					.Style(&FAppStyle::Get(), "Menu.SpinBox")
					.Font( FAppStyle::GetFontStyle(TEXT("MenuItem.Font")))
					.MinValue(FOVMin)
					.MaxValue(FOVMax)
					.Value_Lambda([ViewportClient]()
					{
						return ViewportClient->ViewFOV;
					})
					.OnValueChanged_Lambda([ViewportClient](float InNewValue)
					{
						ViewportClient->FOVAngle = InNewValue;
						ViewportClient->ViewFOV = InNewValue;
						ViewportClient->Invalidate();
					})
				]
			]
		];
	// clang-format on
}

TSharedRef<SWidget> CreateFarViewPlaneMenuWidget(const TSharedRef<SEditorViewport>& InViewport)
{
	TSharedPtr<FEditorViewportClient> ViewportClient = InViewport->GetViewportClient();

	return
		// clang-format off
		SNew(SBox)
		.HAlign(HAlign_Right)
		[
			SNew(SBox)
			.Padding(FMargin(4.0f, 0.0f, 0.0f, 0.0f))
			.WidthOverride(100.0f)
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::Get().GetBrush("Menu.WidgetBorder"))
				.Padding(FMargin(1.0f))
				[
					SNew(SSpinBox<float>)
					.Style(&FAppStyle::Get(), "Menu.SpinBox")
					.ToolTipText(LOCTEXT("FarViewPlaneTooltip", "Distance to use as the far view plane, or zero to enable an infinite far view plane"))
					.MinValue(0.0f)
					.MaxValue(100000.0f)
					.Font(FAppStyle::GetFontStyle(TEXT("MenuItem.Font")))
					.Value_Lambda([ViewportClient]()
					{
						return ViewportClient->GetFarClipPlaneOverride();
					})
					.OnValueChanged_Lambda([ViewportClient](float InNewValue)
					{
						ViewportClient->OverrideFarClipPlane(InNewValue);
						ViewportClient->Invalidate();
					})
				]
			]
		];
	// clang-format on
}

FToolMenuEntry CreateCameraSubmenu(TWeakPtr<SEditorViewport> InViewport)
{
	return FToolMenuEntry::InitSubMenu(
		"Camera",
		LOCTEXT("CameraSubmenuLabel", "Camera"),
		LOCTEXT("CameraSubmenuTooltip", "Camera options"),
		FNewToolMenuDelegate::CreateLambda(
			[InViewport](UToolMenu* Submenu) -> void
			{
				FToolMenuSection& UnnamedSection = Submenu->FindOrAddSection("", LOCTEXT("UnnamedLabel", ""));

				UnnamedSection.AddEntry(FToolMenuEntry::InitWidget(
					"CameraMenuItems", UE::UnrealEd::CreateCameraMenuWidget(InViewport.Pin().ToSharedRef()), FText(), true
				));

				UnnamedSection.AddSeparator("CameraSubmenuSeparator");

				UnnamedSection.AddEntry(FToolMenuEntry::InitWidget(
					"CameraFOV",
					UE::UnrealEd::CreateFOVMenuWidget(InViewport.Pin().ToSharedRef()),
					LOCTEXT("CameraSubmenu_FieldOfViewLabel", "Field of View"),
					true
				));

				UnnamedSection.AddEntry(FToolMenuEntry::InitWidget(
					"CameraFarViewPlane",
					UE::UnrealEd::CreateFarViewPlaneMenuWidget(InViewport.Pin().ToSharedRef()),
					LOCTEXT("CameraSubmenu_FarViewPlaneLabel", "Far View Plane"),
					true
				));
			}
		)
	);
}

static FFormatNamedArguments GetScreenPercentageFormatArguments(const FEditorViewportClient& ViewportClient)
{
	static auto CVarEditorViewportDefaultScreenPercentageRealTimeMode =
		IConsoleManager::Get().FindConsoleVariable(TEXT("r.Editor.Viewport.ScreenPercentageMode.RealTime"));
	static auto CVarEditorViewportDefaultScreenPercentageMobileMode =
		IConsoleManager::Get().FindConsoleVariable(TEXT("r.Editor.Viewport.ScreenPercentageMode.Mobile"));
	static auto CVarEditorViewportDefaultScreenPercentageVRMode =
		IConsoleManager::Get().FindConsoleVariable(TEXT("r.Editor.Viewport.ScreenPercentageMode.VR"));
	static auto CVarEditorViewportDefaultScreenPercentagePathTracerMode =
		IConsoleManager::Get().FindConsoleVariable(TEXT("r.Editor.Viewport.ScreenPercentageMode.PathTracer"));
	static auto CVarEditorViewportDefaultScreenPercentageMode =
		IConsoleManager::Get().FindConsoleVariable(TEXT("r.Editor.Viewport.ScreenPercentageMode.NonRealTime"));

	const UEditorPerformanceProjectSettings* EditorProjectSettings = GetDefault<UEditorPerformanceProjectSettings>();
	const UEditorPerformanceSettings* EditorUserSettings = GetDefault<UEditorPerformanceSettings>();
	const FEngineShowFlags& EngineShowFlags = ViewportClient.EngineShowFlags;

	const EViewStatusForScreenPercentage ViewportRenderingMode = ViewportClient.GetViewStatusForScreenPercentage();
	const bool bViewModeSupportsScreenPercentage = ViewportClient.SupportsPreviewResolutionFraction();
	const bool bIsPreviewScreenPercentage = ViewportClient.IsPreviewingScreenPercentage();

	float DefaultScreenPercentage = FMath::Clamp(
										ViewportClient.GetDefaultPrimaryResolutionFractionTarget(),
										ISceneViewFamilyScreenPercentage::kMinTSRResolutionFraction,
										ISceneViewFamilyScreenPercentage::kMaxTSRResolutionFraction
									)
								  * 100.0f;
	float PreviewScreenPercentage = float(ViewportClient.GetPreviewScreenPercentage());
	float FinalScreenPercentage = bIsPreviewScreenPercentage ? PreviewScreenPercentage : DefaultScreenPercentage;

	FFormatNamedArguments FormatArguments;
	FormatArguments.Add(TEXT("ViewportMode"), UEnum::GetDisplayValueAsText(ViewportRenderingMode));

	EScreenPercentageMode ProjectSetting = EScreenPercentageMode::Manual;
	EEditorUserScreenPercentageModeOverride UserPreference = EEditorUserScreenPercentageModeOverride::ProjectDefault;
	IConsoleVariable* CVarDefaultScreenPercentage = nullptr;
	if (ViewportRenderingMode == EViewStatusForScreenPercentage::PathTracer)
	{
		ProjectSetting = EditorProjectSettings->PathTracerScreenPercentageMode;
		UserPreference = EditorUserSettings->PathTracerScreenPercentageMode;
		CVarDefaultScreenPercentage = CVarEditorViewportDefaultScreenPercentagePathTracerMode;
	}
	else if (ViewportRenderingMode == EViewStatusForScreenPercentage::VR)
	{
		ProjectSetting = EditorProjectSettings->VRScreenPercentageMode;
		UserPreference = EditorUserSettings->VRScreenPercentageMode;
		CVarDefaultScreenPercentage = CVarEditorViewportDefaultScreenPercentageVRMode;
	}
	else if (ViewportRenderingMode == EViewStatusForScreenPercentage::Mobile)
	{
		ProjectSetting = EditorProjectSettings->MobileScreenPercentageMode;
		UserPreference = EditorUserSettings->MobileScreenPercentageMode;
		CVarDefaultScreenPercentage = CVarEditorViewportDefaultScreenPercentageMobileMode;
	}
	else if (ViewportRenderingMode == EViewStatusForScreenPercentage::Desktop)
	{
		ProjectSetting = EditorProjectSettings->RealtimeScreenPercentageMode;
		UserPreference = EditorUserSettings->RealtimeScreenPercentageMode;
		CVarDefaultScreenPercentage = CVarEditorViewportDefaultScreenPercentageRealTimeMode;
	}
	else if (ViewportRenderingMode == EViewStatusForScreenPercentage::NonRealtime)
	{
		ProjectSetting = EditorProjectSettings->NonRealtimeScreenPercentageMode;
		UserPreference = EditorUserSettings->NonRealtimeScreenPercentageMode;
		CVarDefaultScreenPercentage = CVarEditorViewportDefaultScreenPercentageMode;
	}
	else
	{
		unimplemented();
	}

	EScreenPercentageMode FinalScreenPercentageMode = EScreenPercentageMode::Manual;
	if (!bViewModeSupportsScreenPercentage)
	{
		FormatArguments.Add(
			TEXT("SettingSource"), LOCTEXT("ScreenPercentage_SettingSource_UnsupportedByViewMode", "Unsupported by View mode")
		);
		FinalScreenPercentageMode = EScreenPercentageMode::Manual;
		FinalScreenPercentage = 100;
	}
	else if (bIsPreviewScreenPercentage)
	{
		FormatArguments.Add(
			TEXT("SettingSource"), LOCTEXT("ScreenPercentage_SettingSource_ViewportOverride", "Viewport Override")
		);
		FinalScreenPercentageMode = EScreenPercentageMode::Manual;
	}
	else if ((CVarDefaultScreenPercentage->GetFlags() & ECVF_SetByMask) > ECVF_SetByProjectSetting)
	{
		FormatArguments.Add(TEXT("SettingSource"), LOCTEXT("ScreenPercentage_SettingSource_Cvar", "Console Variable"));
		FinalScreenPercentageMode = EScreenPercentageMode(CVarDefaultScreenPercentage->GetInt());
	}
	else if (UserPreference == EEditorUserScreenPercentageModeOverride::ProjectDefault)
	{
		FormatArguments.Add(
			TEXT("SettingSource"), LOCTEXT("ScreenPercentage_SettingSource_ProjectSettigns", "Project Settings")
		);
		FinalScreenPercentageMode = ProjectSetting;
	}
	else
	{
		FormatArguments.Add(
			TEXT("SettingSource"), LOCTEXT("ScreenPercentage_SettingSource_EditorPreferences", "Editor Preferences")
		);
		if (UserPreference == EEditorUserScreenPercentageModeOverride::BasedOnDPIScale)
		{
			FinalScreenPercentageMode = EScreenPercentageMode::BasedOnDPIScale;
		}
		else if (UserPreference == EEditorUserScreenPercentageModeOverride::BasedOnDisplayResolution)
		{
			FinalScreenPercentageMode = EScreenPercentageMode::BasedOnDisplayResolution;
		}
		else
		{
			FinalScreenPercentageMode = EScreenPercentageMode::Manual;
		}
	}

	if (FinalScreenPercentageMode == EScreenPercentageMode::BasedOnDPIScale)
	{
		FormatArguments.Add(TEXT("Setting"), LOCTEXT("ScreenPercentage_Setting_BasedOnDPIScale", "Based on OS's DPI scale"));
	}
	else if (FinalScreenPercentageMode == EScreenPercentageMode::BasedOnDisplayResolution)
	{
		FormatArguments.Add(
			TEXT("Setting"), LOCTEXT("ScreenPercentage_Setting_BasedOnDisplayResolution", "Based on display resolution")
		);
	}
	else
	{
		FormatArguments.Add(TEXT("Setting"), LOCTEXT("ScreenPercentage_Setting_Manual", "Manual"));
	}

	FormatArguments.Add(
		TEXT("CurrentScreenPercentage"),
		FText::FromString(FString::Printf(TEXT("%3.1f"), FMath::RoundToFloat(FinalScreenPercentage * 10.0f) / 10.0f))
	);

	{
		float FinalResolutionFraction = (FinalScreenPercentage / 100.0f);
		FIntPoint DisplayResolution = ViewportClient.Viewport->GetSizeXY();
		FIntPoint RenderingResolution;
		RenderingResolution.X = FMath::CeilToInt(DisplayResolution.X * FinalResolutionFraction);
		RenderingResolution.Y = FMath::CeilToInt(DisplayResolution.Y * FinalResolutionFraction);

		FormatArguments.Add(
			TEXT("ResolutionFromTo"),
			FText::FromString(FString::Printf(
				TEXT("%dx%d -> %dx%d"),
				RenderingResolution.X,
				RenderingResolution.Y,
				DisplayResolution.X,
				DisplayResolution.Y
			))
		);
	}

	return FormatArguments;
}

static const FMargin ScreenPercentageMenuCommonPadding(26.0f, 3.0f);

TSharedRef<SWidget> CreateCurrentPercentageWidget(FEditorViewportClient& InViewportClient)
{
	// clang-format off
	return SNew(SBox)
		.Padding(ScreenPercentageMenuCommonPadding)
		[
			SNew(STextBlock)
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			.Text_Lambda([&InViewportClient]()
			{
				FFormatNamedArguments FormatArguments = GetScreenPercentageFormatArguments(InViewportClient);
				return FText::Format(LOCTEXT("ScreenPercentageCurrent_Display", "Current Screen Percentage: {CurrentScreenPercentage}"), FormatArguments);
			})
			.ToolTip(SNew(SToolTip).Text(LOCTEXT("ScreenPercentageCurrent_ToolTip", "Current Screen Percentage the viewport is rendered with. The primary screen percentage can either be a spatial or temporal upscaler based of your anti-aliasing settings.")))
		];
	// clang-format on
}

TSharedRef<SWidget> CreateResolutionsWidget(FEditorViewportClient& InViewportClient)
{
	// clang-format off
	return SNew(SBox)
	.Padding(ScreenPercentageMenuCommonPadding)
	[
		SNew(STextBlock)
		.ColorAndOpacity(FSlateColor::UseSubduedForeground())
		.Text_Lambda([&InViewportClient]()
		{
			FFormatNamedArguments FormatArguments = GetScreenPercentageFormatArguments(InViewportClient);
			return FText::Format(LOCTEXT("ScreenPercentageResolutions", "Resolution: {ResolutionFromTo}"), FormatArguments);
		})
	];
	// clang-format on
}

TSharedRef<SWidget> CreateActiveViewportWidget(FEditorViewportClient& InViewPortClient)
{
	// clang-format off
	return SNew(SBox)
		.Padding(ScreenPercentageMenuCommonPadding)
		[
			SNew(STextBlock)
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			.Text_Lambda([&InViewPortClient]()
			{
				FFormatNamedArguments FormatArguments = GetScreenPercentageFormatArguments(InViewPortClient);
				return FText::Format(LOCTEXT("ScreenPercentageActiveViewport", "Active Viewport: {ViewportMode}"), FormatArguments);
			})
		];
	// clang-format on
}

TSharedRef<SWidget> CreateSetFromWidget(FEditorViewportClient& InViewPortClient)
{
	// clang-format off
	return SNew(SBox)
		.Padding(ScreenPercentageMenuCommonPadding)
		[
			SNew(STextBlock)
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			.Text_Lambda([&InViewPortClient]()
			{
				FFormatNamedArguments FormatArguments = GetScreenPercentageFormatArguments(InViewPortClient);
				return FText::Format(LOCTEXT("ScreenPercentageSetFrom", "Set From: {SettingSource}"), FormatArguments);
			})
		];
	// clang-format on
}

TSharedRef<SWidget> CreateCurrentScreenPercentageSettingWidget(FEditorViewportClient& InViewPortClient)
{
	// clang-format off
	return SNew(SBox)
		.Padding(ScreenPercentageMenuCommonPadding)
		[
			SNew(STextBlock)
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			.Text_Lambda([&InViewPortClient]()
			{
				FFormatNamedArguments FormatArguments = GetScreenPercentageFormatArguments(InViewPortClient);
				return FText::Format(LOCTEXT("ScreenPercentageSetting", "Setting: {Setting}"), FormatArguments);
			})
		];
	// clang-format on
}

TSharedRef<SWidget> CreateCurrentScreenPercentageWidget(FEditorViewportClient& InViewPortClient)
{
	constexpr int32 PreviewScreenPercentageMin = ISceneViewFamilyScreenPercentage::kMinTSRResolutionFraction * 100.0f;
	constexpr int32 PreviewScreenPercentageMax = ISceneViewFamilyScreenPercentage::kMaxTSRResolutionFraction * 100.0f;

	// clang-format off
	return SNew(SBox)
		.HAlign(HAlign_Right)
		.IsEnabled_Lambda([&InViewPortClient]()
		{
			return InViewPortClient.IsPreviewingScreenPercentage() && InViewPortClient.SupportsPreviewResolutionFraction();
		})
		[
			SNew(SBox)
			.Padding(FMargin(4.0f, 0.0f, 0.0f, 0.0f))
			.WidthOverride(100.0f)
			[
				SNew(SBorder)
				.Padding(FMargin(1.0f))
				[
					SNew(SSpinBox<int32>)
					.Style(&FAppStyle::Get(), "Menu.SpinBox")
					.Font(FAppStyle::GetFontStyle(TEXT("MenuItem.Font")))
					.MinSliderValue(PreviewScreenPercentageMin)
					.MaxSliderValue(PreviewScreenPercentageMax)
					.Value_Lambda([&InViewPortClient]()
					{
						return InViewPortClient.GetPreviewScreenPercentage();
					})
					.OnValueChanged_Lambda([&InViewPortClient](int32 NewValue)
					{
						InViewPortClient.SetPreviewScreenPercentage(NewValue);
						InViewPortClient.Invalidate();
					})
				]
			]
		];
	// clang-format on
}

static void ConstructScreenPercentageMenu(UToolMenu* InMenu, FEditorViewportClient* InViewportClient)
{
	FEditorViewportClient& ViewportClient = *InViewportClient;

	const FEditorViewportCommands& BaseViewportCommands = FEditorViewportCommands::Get();

	// Summary
	{
		FToolMenuSection& SummarySection = InMenu->FindOrAddSection("Summary", LOCTEXT("Summary", "Summary"));
		SummarySection.AddEntry(FToolMenuEntry::InitWidget(
			"ScreenPercentageCurrent", CreateCurrentPercentageWidget(ViewportClient), FText::GetEmpty()
		));

		SummarySection.AddEntry(FToolMenuEntry::InitWidget(
			"ScreenPercentageResolutions", CreateResolutionsWidget(ViewportClient), FText::GetEmpty()
		));

		SummarySection.AddEntry(FToolMenuEntry::InitWidget(
			"ScreenPercentageActiveViewport", CreateActiveViewportWidget(ViewportClient), FText::GetEmpty()
		));

		SummarySection.AddEntry(
			FToolMenuEntry::InitWidget("ScreenPercentageSetFrom", CreateSetFromWidget(ViewportClient), FText::GetEmpty())
		);

		SummarySection.AddEntry(FToolMenuEntry::InitWidget(
			"ScreenPercentageSetting", CreateCurrentScreenPercentageSettingWidget(ViewportClient), FText::GetEmpty()
		));
	}

	// Screen Percentage
	{
		FToolMenuSection& ScreenPercentageSection =
			InMenu->FindOrAddSection("ScreenPercentage", LOCTEXT("ScreenPercentage_ViewportOverride", "Viewport Override"));

		ScreenPercentageSection.AddMenuEntry(BaseViewportCommands.ToggleOverrideViewportScreenPercentage);

		ScreenPercentageSection.AddEntry(FToolMenuEntry::InitWidget(
			"PreviewScreenPercentage",
			CreateCurrentScreenPercentageWidget(ViewportClient),
			LOCTEXT("ScreenPercentage", "Screen Percentage")
		));
	}

	// Screen Percentage Settings
	{
		FToolMenuSection& ScreenPercentageSettingsSection = InMenu->FindOrAddSection(
			"ScreenPercentageSettings", LOCTEXT("ScreenPercentage_ViewportSettings", "Viewport Settings")
		);

		ScreenPercentageSettingsSection.AddMenuEntry(
			BaseViewportCommands.OpenEditorPerformanceProjectSettings,
			/* InLabelOverride = */ TAttribute<FText>(),
			/* InToolTipOverride = */ TAttribute<FText>(),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "ProjectSettings.TabIcon")
		);

		ScreenPercentageSettingsSection.AddMenuEntry(
			BaseViewportCommands.OpenEditorPerformanceEditorPreferences,
			/* InLabelOverride = */ TAttribute<FText>(),
			/* InToolTipOverride = */ TAttribute<FText>(),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "EditorPreferences.TabIcon")
		);
	}
}

UNREALED_API FToolMenuEntry CreatePerformanceAndScalabilitySubmenu(TWeakPtr<SEditorViewport> InViewport)
{
	return FToolMenuEntry::InitSubMenu(
		"PerformanceAndScalability",
		LOCTEXT("PerformanceAndScalabilitySubmenuLabel", "Performance and Scalability"),
		LOCTEXT("PerformanceAndScalabilitySubmenuTooltip", ""),
		FNewToolMenuDelegate::CreateLambda(
			[InViewport](UToolMenu* Submenu) -> void
			{
				TSharedPtr<SEditorViewport> Viewport = InViewport.Pin();
				if (!Viewport)
				{
					return;
				}

				FToolMenuSection& UnnamedSection = Submenu->FindOrAddSection("", LOCTEXT("UnnamedLabel", ""));

				UnnamedSection.AddMenuEntry(FEditorViewportCommands::Get().ToggleRealTime);

				TSharedPtr<FEditorViewportClient> ViewportClient = Viewport->GetViewportClient();

				UnnamedSection.AddSubMenu(
					"ScreenPercentage",
					LOCTEXT("ScreenPercentageSubMenu", "Screen Percentage"),
					LOCTEXT("ScreenPercentageSubMenu_ToolTip", "Customize the viewport's screen percentage"),
					FNewToolMenuDelegate::CreateStatic(&ConstructScreenPercentageMenu, ViewportClient.Get())
				);
			}
		)
	);
}

} // namespace UE::UnrealEd

#undef LOCTEXT_NAMESPACE
