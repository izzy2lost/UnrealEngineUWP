// Copyright Epic Games, Inc. All Rights Reserved.

#include "ViewportToolbar/LevelEditorViewportToolbarSections.h"

#include "EditorViewportCommands.h"
#include "FoliageType.h"
#include "Framework/Commands/GenericCommands.h"
#include "GameFramework/ActorPrimitiveColorHandler.h"
#include "GroomVisualizationData.h"
#include "Layers/LayersSubsystem.h"
#include "LevelEditor.h"
#include "LevelEditorActions.h"
#include "LevelViewportActions.h"
#include "SCommonEditorViewportToolbarBase.h"
#include "SLevelViewport.h"
#include "SScalabilitySettings.h"
#include "ShowFlagMenuCommands.h"
#include "Stats/StatsData.h"
#include "Templates/SharedPointer.h"
#include "ToolMenu.h"
#include "ToolMenus.h"
#include "ViewportToolbar/LevelViewportContext.h"
#include "ViewportToolbar/UnrealEdViewportToolbar.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Input/SVolumeControl.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SToolTip.h"
#include "WorldPartition/IWorldPartitionEditorModule.h"

#define LOCTEXT_NAMESPACE "LevelEditorViewportToolbar"

namespace UE::LevelEditor::Private
{

bool IsLandscapeLODSettingChecked(FLevelEditorViewportClient& ViewportClient, int32 Value)
{
	return ViewportClient.LandscapeLODOverride == Value;
}

void OnLandscapeLODChanged(FLevelEditorViewportClient& ViewportClient, int32 NewValue)
{
	ViewportClient.LandscapeLODOverride = NewValue;
	ViewportClient.Invalidate();
}

TMap<FName, TArray<UFoliageType*>> GroupFoliageByOuter(const TArray<UFoliageType*> FoliageList)
{
	TMap<FName, TArray<UFoliageType*>> Result;

	for (UFoliageType* FoliageType : FoliageList)
	{
		if (FoliageType->IsAsset())
		{
			Result.FindOrAdd(NAME_None).Add(FoliageType);
		}
		else
		{
			FName LevelName = FoliageType->GetOutermost()->GetFName();
			Result.FindOrAdd(LevelName).Add(FoliageType);
		}
	}

	Result.KeySort(
		[](const FName& A, const FName& B)
		{
			return (A.LexicalLess(B) && B != NAME_None);
		}
	);
	return Result;
}

void PopulateMenuWithCommands(UToolMenu* Menu, TArray<FLevelViewportCommands::FShowMenuCommand> MenuCommands, int32 EntryOffset)
{
	FToolMenuSection& Section = Menu->AddSection("Section");

	// Generate entries for the standard show flags
	// Assumption: the first 'n' entries types like 'Show All' and 'Hide All' buttons, so insert a separator after them
	for (int32 EntryIndex = 0; EntryIndex < MenuCommands.Num(); ++EntryIndex)
	{
		FName EntryName = NAME_None;

		if (MenuCommands[EntryIndex].ShowMenuItem)
		{
			EntryName = MenuCommands[EntryIndex].ShowMenuItem->GetCommandName();
			ensure(Section.FindEntry(EntryName) == nullptr);
		}

		Section.AddMenuEntry(EntryName, MenuCommands[EntryIndex].ShowMenuItem, MenuCommands[EntryIndex].LabelOverride);

		if (EntryIndex == EntryOffset - 1)
		{
			Section.AddSeparator(NAME_None);
		}
	}
}

void PopulateShowLayersSubmenu(UToolMenu* InMenu, TWeakPtr<::SLevelViewport> InViewport)
{
	{
		FToolMenuSection& Section = InMenu->AddSection("LevelViewportLayers");
		Section.AddMenuEntry(FLevelViewportCommands::Get().ShowAllLayers, LOCTEXT("ShowAllLabel", "Show All"));
		Section.AddMenuEntry(FLevelViewportCommands::Get().HideAllLayers, LOCTEXT("HideAllLabel", "Hide All"));
	}

	if (TSharedPtr<::SLevelViewport> ViewportPinned = InViewport.Pin())
	{
		FToolMenuSection& Section = InMenu->AddSection("LevelViewportLayers2");
		// Get all the layers and create an entry for each of them
		TArray<FName> AllLayerNames;
		ULayersSubsystem* Layers = GEditor->GetEditorSubsystem<ULayersSubsystem>();
		Layers->AddAllLayerNamesTo(AllLayerNames);

		for (int32 LayerIndex = 0; LayerIndex < AllLayerNames.Num(); ++LayerIndex)
		{
			const FName LayerName = AllLayerNames[LayerIndex];
			// const FString LayerNameString = LayerName;

			FUIAction Action(
				FExecuteAction::CreateSP(ViewportPinned.ToSharedRef(), &::SLevelViewport::ToggleShowLayer, LayerName),
				FCanExecuteAction(),
				FIsActionChecked::CreateSP(ViewportPinned.ToSharedRef(), &::SLevelViewport::IsLayerVisible, LayerName)
			);

			Section.AddMenuEntry(
				NAME_None, FText::FromName(LayerName), FText::GetEmpty(), FSlateIcon(), Action, EUserInterfaceActionType::ToggleButton
			);
		}
	}
}

// TODO: Maybe export CreateSurfaceSnapOffsetEntry function, so that it can be used elsewhere, e.g. STransformViewportToolbar.cpp
FToolMenuEntry CreateSurfaceSnapOffsetEntry()
{
	FText Label = LOCTEXT("SurfaceOffsetLabel", "Surface Offset");
	FText Tooltip = LOCTEXT("SurfaceOffsetTooltip", "The amount of offset to apply when snapping to surfaces");

	FToolMenuEntry SurfaceOffset = FToolMenuEntry::InitMenuEntry(
		"SurfaceOffset",
		Label,
		Tooltip,
		FSlateIcon(),
		FUIAction());

	const FMargin WidgetsMargin(8.0f, 0.0f, 0.0f, 0.0f);

	SurfaceOffset.MakeCustomWidget.BindLambda(
		[Label, Tooltip, WidgetsMargin](const FToolMenuContext& InContext, const FToolMenuCustomWidgetContext& InWidgetContext)
		{
			// clang-format on
			return SNew(SHorizontalBox).IsEnabled_Lambda([]()
				{
					return GetDefault<ULevelEditorViewportSettings>()->SnapToSurface.bEnabled;
				})
				+ SHorizontalBox::Slot().VAlign(VAlign_Center).Padding(WidgetsMargin).AutoWidth()
				[
					SNew(STextBlock).Text(Label).TextStyle(InWidgetContext.StyleSet,
					                                       ISlateStyle::Join(InWidgetContext.StyleName, ".Label"))
				]
				+ SHorizontalBox::Slot().VAlign(VAlign_Center).Padding(WidgetsMargin).AutoWidth()
				[
					SNew(SBox).Padding(WidgetsMargin).MinDesiredWidth(100.0f)
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
							auto* Settings = GetMutableDefault<ULevelEditorViewportSettings>();
							Settings->SnapToSurface.SnapOffsetExtent = InNewValue;
						})
						.Value_Lambda([]()
						{
							return GetDefault<ULevelEditorViewportSettings>()->SnapToSurface.SnapOffsetExtent;
						})
					]
				];
			// clang-format off
		});

	return SurfaceOffset;
}

FToolMenuEntry CreateSurfaceSnapMenu()
{
	FNewToolMenuDelegate MakeMenuDelegate = FNewToolMenuDelegate::CreateLambda([](UToolMenu* Submenu)
	{
		FToolMenuSection& SurfaceSnappingSection = Submenu->FindOrAddSection("SurfaceSnapping", LOCTEXT("SurfaceSnappingLabel", "Surface Snapping"));

		// Add "Rotate to surface normal" checkbox.
		{
			FToolMenuEntry RotateToSurfaceNormalSnapping = FToolMenuEntry::InitMenuEntry(FEditorViewportCommands::Get().RotateToSurfaceNormal);
			SurfaceSnappingSection.AddEntry(RotateToSurfaceNormalSnapping);
		}

		// Add "Surface offset" widget.
		{
			SurfaceSnappingSection.AddEntry(CreateSurfaceSnapOffsetEntry());
		}
	});

	FToolUIAction CheckboxMenuAction;
	{
		CheckboxMenuAction.ExecuteAction = FToolMenuExecuteAction::CreateLambda(
			[](const FToolMenuContext& InContext)
			{
				auto* Settings = GetMutableDefault<ULevelEditorViewportSettings>();
				Settings->SnapToSurface.bEnabled = !Settings->SnapToSurface.bEnabled;
			}
		);
		CheckboxMenuAction.GetActionCheckState = FToolMenuGetActionCheckState::CreateLambda(
			[](const FToolMenuContext& InContext)
			{
				return GetDefault<ULevelEditorViewportSettings>()->SnapToSurface.bEnabled ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
			}
		);
	}

	FToolMenuEntry SurfaceSnapping = FToolMenuEntry::InitSubMenu(
		"SurfaceSnapping",
		LOCTEXT("SurfaceSnapLabel", "Surface"),
		FEditorViewportCommands::Get().SurfaceSnapping->MakeTooltip()->GetTextTooltip(),
		MakeMenuDelegate,
		CheckboxMenuAction,
		EUserInterfaceActionType::ToggleButton
	);

	return SurfaceSnapping;
}

FToolMenuEntry CreateActorSnapDistanceEntry()
{
	const FText Label = LOCTEXT("ActorSnapDistanceLabel", "Snap Distance");
	const FText Tooltip = LOCTEXT("ActorSnapDistanceTooltip", "The amount of offset to apply when snapping to surfaces");

	FToolMenuEntry SnapDistance = FToolMenuEntry::InitMenuEntry(
		"ActorSnapDistance",
		Label,
		Tooltip,
		FSlateIcon(),
		FUIAction());

	const FMargin WidgetsMargin(8.0f, 0.0f, 0.0f, 0.0f);

	SnapDistance.MakeCustomWidget.BindLambda(
		[Label, Tooltip, WidgetsMargin](const FToolMenuContext& InContext, const FToolMenuCustomWidgetContext& InWidgetContext)
		{
			// clang-format on
			return SNew(SHorizontalBox).IsEnabled_Lambda([]()
				{
					return !!GetDefault<ULevelEditorViewportSettings>()->bEnableActorSnap;
				})
				+ SHorizontalBox::Slot().VAlign(VAlign_Center).Padding(WidgetsMargin).AutoWidth()
				[
					SNew(STextBlock).Text(Label).TextStyle(InWidgetContext.StyleSet,
					                                       ISlateStyle::Join(InWidgetContext.StyleName, ".Label"))
				]
				+ SHorizontalBox::Slot().VAlign(VAlign_Center).Padding(WidgetsMargin).AutoWidth()
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
				];
			// clang-format off
		});

	return SnapDistance;
}

FToolMenuEntry CreateActorSnapMenu()
{
	FNewToolMenuDelegate MakeMenuDelegate = FNewToolMenuDelegate::CreateLambda([](UToolMenu* Submenu)
	{
		FToolMenuSection& ActorSnappingSection = Submenu->FindOrAddSection("ActorSnapping", LOCTEXT("ActorSnappingLabel", "Actor Snapping"));

		// Add "Actor snapping" widget.
		{
			ActorSnappingSection.AddEntry(CreateActorSnapDistanceEntry());
		}
	});

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
				return GetDefault<ULevelEditorViewportSettings>()->bEnableActorSnap ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
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

} // namespace UE::LevelEditor::Private

namespace UE::LevelEditor
{

bool ShowViewportRealtimeWarning(FLevelEditorViewportClient& ViewportClient)
{
	return !ViewportClient.IsRealtime() && !ViewportClient.IsRealtimeOverrideSet() && ViewportClient.IsPerspective();
}

// TODO: Move this outside the level editor and make it publicly available to anyone building a viewport toolbar.
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

					UnnamedSection.AddMenuEntry(FGenericCommands::Get().SelectAll);
					UnnamedSection.AddMenuEntry(FLevelEditorCommands::Get().SelectNone);
					UnnamedSection.AddMenuEntry(FLevelEditorCommands::Get().InvertSelection);

					UnnamedSection.AddSeparator("Advanced");

					UnnamedSection.AddMenuEntry(FLevelEditorCommands::Get().SelectAllActorsOfSameClass);
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

								SelectAllSection.AddMenuEntry(FLevelEditorCommands::Get().SelectAllAddditiveBrushes);
								SelectAllSection.AddMenuEntry(FLevelEditorCommands::Get().SelectAllSubtractiveBrushes);
								SelectAllSection.AddMenuEntry(FLevelEditorCommands::Get().SelectAllSurfaces);
							}
						)
					);
				}

				{
					FToolMenuSection& OptionsSection =
						Submenu->FindOrAddSection("Options", LOCTEXT("OptionsLabel", "Options"));

					OptionsSection.AddMenuEntry(FLevelEditorCommands::Get().AllowTranslucentSelection);
				}
			}
		)
	);
}

TSharedPtr<FExtender> GetViewModesLegacyExtenders()
{
	FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
	return LevelEditorModule.GetMenuExtensibilityManager()->GetAllExtenders();
}

void PopulateViewModesMenu(UToolMenu* InMenu, TSharedRef<::SLevelViewport> InViewport)
{
	FToolMenuInsert InsertPosition("ViewMode", EToolMenuInsertType::After);

	{
		FToolMenuSection& Section = InMenu->AddSection(
			"LevelViewportDeferredRendering", LOCTEXT("DeferredRenderingHeader", "Deferred Rendering"), InsertPosition
		);
	}

	{
		FToolMenuSection& Section = InMenu->FindOrAddSection("ViewMode");
		Section.AddSubMenu(
			"VisualizeBufferViewMode",
			LOCTEXT("VisualizeBufferViewModeDisplayName", "Buffer Visualization"),
			LOCTEXT("BufferVisualizationMenu_ToolTip", "Select a mode for buffer visualization"),
			FNewMenuDelegate::CreateStatic(&FBufferVisualizationMenuCommands::BuildVisualisationSubMenu),
			FUIAction(
				FExecuteAction(),
				FCanExecuteAction(),
				FIsActionChecked::CreateLambda(
					[WeakViewport = InViewport.ToWeakPtr()]()
					{
						const TSharedPtr<::SLevelViewport> Viewport = WeakViewport.Pin();
						check(Viewport.IsValid());
						FLevelEditorViewportClient& ViewportClient = Viewport->GetLevelViewportClient();
						return ViewportClient.IsViewModeEnabled(VMI_VisualizeBuffer);
					}
				)
			),
			EUserInterfaceActionType::RadioButton,
			/* bInOpenSubMenuOnClick = */ false,
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "EditorViewport.VisualizeBufferMode")
		);
	}

	{
		FToolMenuSection& Section = InMenu->FindOrAddSection("ViewMode");
		Section.AddSubMenu(
			"VisualizeNaniteViewMode",
			LOCTEXT("VisualizeNaniteViewModeDisplayName", "Nanite Visualization"),
			LOCTEXT("NaniteVisualizationMenu_ToolTip", "Select a mode for Nanite visualization"),
			FNewMenuDelegate::CreateStatic(&FNaniteVisualizationMenuCommands::BuildVisualisationSubMenu),
			FUIAction(
				FExecuteAction(),
				FCanExecuteAction(),
				FIsActionChecked::CreateLambda(
					[WeakViewport = InViewport.ToWeakPtr()]()
					{
						const TSharedPtr<::SLevelViewport> Viewport = WeakViewport.Pin();
						check(Viewport.IsValid());
						FLevelEditorViewportClient& ViewportClient = Viewport->GetLevelViewportClient();
						return ViewportClient.IsViewModeEnabled(VMI_VisualizeNanite);
					}
				)
			),
			EUserInterfaceActionType::RadioButton,
			/* bInOpenSubMenuOnClick = */ false,
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "EditorViewport.VisualizeNaniteMode")
		);
	}

	{
		FToolMenuSection& Section = InMenu->FindOrAddSection("ViewMode");
		Section.AddSubMenu(
			"VisualizeLumenViewMode",
			LOCTEXT("VisualizeLumenViewModeDisplayName", "Lumen"),
			LOCTEXT("LumenVisualizationMenu_ToolTip", "Select a mode for Lumen visualization"),
			FNewMenuDelegate::CreateStatic(&FLumenVisualizationMenuCommands::BuildVisualisationSubMenu),
			FUIAction(
				FExecuteAction(),
				FCanExecuteAction(),
				FIsActionChecked::CreateLambda(
					[WeakViewport = InViewport.ToWeakPtr()]()
					{
						const TSharedPtr<::SLevelViewport> Viewport = WeakViewport.Pin();
						check(Viewport.IsValid());
						FLevelEditorViewportClient& ViewportClient = Viewport->GetLevelViewportClient();
						return ViewportClient.IsViewModeEnabled(VMI_VisualizeLumen);
					}
				)
			),
			EUserInterfaceActionType::RadioButton,
			/* bInOpenSubMenuOnClick = */ false,
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "EditorViewport.VisualizeLumenMode")
		);
	}

	if (Substrate::IsSubstrateEnabled())
	{
		FToolMenuSection& Section = InMenu->FindOrAddSection("ViewMode");
		Section.AddSubMenu(
			"VisualizeSubstrateViewMode",
			LOCTEXT("VisualizeSubstrateViewModeDisplayName", "Substrate"),
			LOCTEXT("SubstrateVisualizationMenu_ToolTip", "Select a mode for Substrate visualization"),
			FNewMenuDelegate::CreateStatic(&FSubstrateVisualizationMenuCommands::BuildVisualisationSubMenu),
			FUIAction(
				FExecuteAction(),
				FCanExecuteAction(),
				FIsActionChecked::CreateLambda(
					[WeakViewport = InViewport.ToWeakPtr()]()
					{
						const TSharedPtr<::SLevelViewport> Viewport = WeakViewport.Pin();
						check(Viewport.IsValid());
						FLevelEditorViewportClient& ViewportClient = Viewport->GetLevelViewportClient();
						return ViewportClient.IsViewModeEnabled(VMI_VisualizeSubstrate);
					}
				)
			),
			EUserInterfaceActionType::RadioButton,
			/* bInOpenSubMenuOnClick = */ false,
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "EditorViewport.VisualizeSubstrateMode")
		);
	}

	if (IsGroomEnabled())
	{
		FToolMenuSection& Section = InMenu->FindOrAddSection("ViewMode");
		Section.AddSubMenu(
			"VisualizeGroomViewMode",
			LOCTEXT("VisualizeGroomViewModeDisplayName", "Groom"),
			LOCTEXT("GroomVisualizationMenu_ToolTip", "Select a mode for Groom visualization"),
			FNewMenuDelegate::CreateStatic(&FGroomVisualizationMenuCommands::BuildVisualisationSubMenu),
			FUIAction(
				FExecuteAction(),
				FCanExecuteAction(),
				FIsActionChecked::CreateLambda(
					[WeakViewport = InViewport.ToWeakPtr()]()
					{
						const TSharedPtr<::SLevelViewport> Viewport = WeakViewport.Pin();
						check(Viewport.IsValid());
						FLevelEditorViewportClient& ViewportClient = Viewport->GetLevelViewportClient();
						return ViewportClient.IsViewModeEnabled(VMI_VisualizeGroom);
					}
				)
			),
			EUserInterfaceActionType::RadioButton,
			/* bInOpenSubMenuOnClick = */ false,
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "EditorViewport.VisualizeGroomMode")
		);
	}

	{
		FToolMenuSection& Section = InMenu->FindOrAddSection("ViewMode");
		Section.AddSubMenu(
			"VisualizeVirtualShadowMapViewMode",
			LOCTEXT("VisualizeVirtualShadowMapViewModeDisplayName", "Virtual Shadow Map"),
			LOCTEXT(
				"VirtualShadowMapVisualizationMenu_ToolTip",
				"Select a mode for virtual shadow map visualization. Select a light component in the world outliner to "
				"visualize that light."
			),
			FNewMenuDelegate::CreateStatic(&FVirtualShadowMapVisualizationMenuCommands::BuildVisualisationSubMenu),
			FUIAction(
				FExecuteAction(),
				FCanExecuteAction(),
				FIsActionChecked::CreateLambda(
					[WeakViewport = InViewport.ToWeakPtr()]()
					{
						const TSharedPtr<::SLevelViewport> Viewport = WeakViewport.Pin();
						check(Viewport.IsValid());
						FLevelEditorViewportClient& ViewportClient = Viewport->GetLevelViewportClient();
						return ViewportClient.IsViewModeEnabled(VMI_VisualizeVirtualShadowMap);
					}
				)
			),
			EUserInterfaceActionType::RadioButton,
			/* bInOpenSubMenuOnClick = */ false,
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "EditorViewport.VisualizeVirtualShadowMapMode")
		);
	}

	{
		auto BuildActorColorationMenu = [WeakViewport = InViewport.ToWeakPtr()](UToolMenu* InMenu)
		{
			FToolMenuSection& SubMenuSection =
				InMenu->AddSection("LevelViewportActorColoration", LOCTEXT("ActorColorationHeader", "Actor Coloration"));

			TArray<FActorPrimitiveColorHandler::FPrimitiveColorHandler> PrimitiveColorHandlers;
			FActorPrimitiveColorHandler::Get().GetRegisteredPrimitiveColorHandlers(PrimitiveColorHandlers);

			for (const FActorPrimitiveColorHandler::FPrimitiveColorHandler& PrimitiveColorHandler : PrimitiveColorHandlers)
			{
				if (!PrimitiveColorHandler.bAvailalbleInEditor)
				{
					continue;
				}

				SubMenuSection.AddMenuEntry(
					NAME_None,
					PrimitiveColorHandler.HandlerText,
					FText(),
					FSlateIcon(),
					FUIAction(
						FExecuteAction::CreateLambda(
							[WeakViewport, PrimitiveColorHandler]()
							{
								if (TSharedPtr<::SLevelViewport> Viewport = WeakViewport.Pin())
								{
									const bool bActorColorationEnabled =
										Viewport->GetLevelViewportClient().HandleIsShowFlagEnabled(
											FEngineShowFlags::EShowFlag::SF_ActorColoration
										);

									if (PrimitiveColorHandler.HandlerName.IsNone())
									{
										if (bActorColorationEnabled)
										{
											Viewport->GetLevelViewportClient().HandleToggleShowFlag(
												FEngineShowFlags::EShowFlag::SF_ActorColoration
											);
										}
									}
									else
									{
										if (!bActorColorationEnabled)
										{
											Viewport->GetLevelViewportClient().HandleToggleShowFlag(
												FEngineShowFlags::EShowFlag::SF_ActorColoration
											);
										}

										FActorPrimitiveColorHandler::Get().SetActivePrimitiveColorHandler(
											PrimitiveColorHandler.HandlerName, GWorld
										);
									}
								}
							}
						),
						FCanExecuteAction::CreateLambda(
							[WeakViewport]()
							{
								if (TSharedPtr<::SLevelViewport> Viewport = WeakViewport.Pin())
								{
									return true;
								}
								return false;
							}
						),
						FGetActionCheckState::CreateLambda(
							[WeakViewport, PrimitiveColorHandler]()
							{
								if (TSharedPtr<::SLevelViewport> Viewport = WeakViewport.Pin())
								{
									const bool bActorColorationEnabled =
										Viewport->GetLevelViewportClient().HandleIsShowFlagEnabled(
											FEngineShowFlags::EShowFlag::SF_ActorColoration
										);

									if (PrimitiveColorHandler.HandlerName.IsNone())
									{
										return bActorColorationEnabled ? ECheckBoxState::Unchecked
																	   : ECheckBoxState::Checked;
									}
									else
									{
										if (bActorColorationEnabled)
										{
											return FActorPrimitiveColorHandler::Get().GetActivePrimitiveColorHandler()
														== PrimitiveColorHandler.HandlerName
													 ? ECheckBoxState::Checked
													 : ECheckBoxState::Unchecked;
										}
									}
								}

								return ECheckBoxState::Unchecked;
							}
						)
					),
					EUserInterfaceActionType::RadioButton
				);
			}
		};

		FToolMenuSection& Section = InMenu->FindOrAddSection("ViewMode");
		Section.AddSubMenu(
			"ActorColoration",
			LOCTEXT("ActorColorationDisplayName", "Actor Coloration"),
			LOCTEXT("ActorColorationMenu_ToolTip", "Override Actor Coloration mode"),
			FNewToolMenuDelegate::CreateLambda(BuildActorColorationMenu),
			FUIAction(
				FExecuteAction(),
				FCanExecuteAction(),
				FIsActionChecked::CreateLambda(
					[WeakViewport = InViewport.ToWeakPtr()]()
					{
						if (const TSharedPtr<::SLevelViewport> Viewport = WeakViewport.Pin())
						{
							return Viewport->GetLevelViewportClient().HandleIsShowFlagEnabled(
								FEngineShowFlags::EShowFlag::SF_ActorColoration
							);
						}
						return false;
					}
				)
			),
			EUserInterfaceActionType::RadioButton,
			/*bInOpenSubMenuOnClick=*/false,
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "EditorViewport.LODColorationMode")
		);
	}

	{
		FToolMenuSection& Section =
			InMenu->AddSection("LevelViewportLandscape", LOCTEXT("LandscapeHeader", "Landscape"), InsertPosition);

		auto BuildLandscapeLODMenu = [WeakViewport = InViewport.ToWeakPtr()](UToolMenu* InMenu)
		{
			FToolMenuSection& SubMenuSection =
				InMenu->AddSection("LevelViewportLandScapeLOD", LOCTEXT("LandscapeLODHeader", "Landscape LOD"));

			auto CreateLandscapeLODAction = [WeakViewport](int32 LODValue)
			{
				FUIAction LandscapeLODAction;
				LandscapeLODAction.ExecuteAction = FExecuteAction::CreateLambda(
					[WeakViewport, LODValue]()
					{
						if (const TSharedPtr<::SLevelViewport> Viewport = WeakViewport.Pin())
						{
							UE::LevelEditor::Private::OnLandscapeLODChanged(Viewport->GetLevelViewportClient(), LODValue);
						}
					}
				);
				LandscapeLODAction.GetActionCheckState = FGetActionCheckState::CreateLambda(
					[WeakViewport, LODValue]() -> ECheckBoxState
					{
						bool bChecked = false;
						if (const TSharedPtr<::SLevelViewport> Viewport = WeakViewport.Pin())
						{
							bChecked = UE::LevelEditor::Private::IsLandscapeLODSettingChecked(
								Viewport->GetLevelViewportClient(), LODValue
							);
						}
						return bChecked ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
					}
				);

				return LandscapeLODAction;
			};

			SubMenuSection.AddMenuEntry(
				"LandscapeLODAuto",
				LOCTEXT("LandscapeLODAuto", "Auto"),
				FText(),
				FSlateIcon(),
				CreateLandscapeLODAction(-1),
				EUserInterfaceActionType::RadioButton
			);

			SubMenuSection.AddSeparator("LandscapeLODSeparator");

			static const FText FormatString = LOCTEXT("LandscapeLODFixed", "Fixed at {0}");
			for (int32 i = 0; i < 8; ++i)
			{
				SubMenuSection.AddMenuEntry(
					NAME_None,
					FText::Format(FormatString, FText::AsNumber(i)),
					FText(),
					FSlateIcon(),
					CreateLandscapeLODAction(i),
					EUserInterfaceActionType::RadioButton
				);
			}
		};

		Section.AddSubMenu(
			"LandscapeLOD",
			LOCTEXT("LandscapeLODDisplayName", "LOD"),
			LOCTEXT("LandscapeLODMenu_ToolTip", "Override Landscape LOD in this viewport"),
			FNewToolMenuDelegate::CreateLambda(BuildLandscapeLODMenu),
			/*bInOpenSubMenuOnClick=*/false,
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "EditorViewport.LOD")
		);
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
				TAttribute<FText> LabelAttribute = UE::UnrealEd::GetViewModesSubmenuLabel(nullptr);
				if (ULevelViewportContext* const LevelViewportContext =
						InDynamicSection.FindContext<ULevelViewportContext>())
				{
					TWeakPtr<SEditorViewport> EditorViewport = LevelViewportContext->LevelViewport;
					LabelAttribute = TAttribute<FText>::CreateLambda(
						[EditorViewport]()
						{
							return UE::UnrealEd::GetViewModesSubmenuLabel(EditorViewport);
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
							ULevelViewportContext* const LevelViewportContext =
								Submenu->FindContext<ULevelViewportContext>();
							if (!LevelViewportContext)
							{
								return;
							}

							if (const TSharedPtr<::SLevelViewport> LevelViewport = LevelViewportContext->LevelViewport.Pin())
							{
								UE::UnrealEd::PopulateViewModesMenu(Submenu, LevelViewport.ToSharedRef());
								PopulateViewModesMenu(Submenu, LevelViewport.ToSharedRef());
							}
						}
					)
				);
			}
		)
	);
}

FToolMenuEntry CreateShowFoliageSubmenu()
{
	return FToolMenuEntry::InitSubMenu(
		"ShowFoliage",
		LOCTEXT("ShowFoliageTypesMenu", "Foliage Types"),
		LOCTEXT("ShowFoliageTypesMenu_ToolTip", "Show/hide specific foliage types"),
		FNewToolMenuDelegate::CreateLambda(
			[](UToolMenu* Submenu)
			{
				ULevelViewportContext* const LevelViewportContext = Submenu->FindContext<ULevelViewportContext>();
				if (!LevelViewportContext)
				{
					return;
				}

				TSharedPtr<::SLevelViewport> Viewport = LevelViewportContext->LevelViewport.Pin();
				if (!Viewport)
				{
					return;
				}

				{
					FToolMenuSection& Section = Submenu->AddSection("LevelViewportFoliageMeshes");
					// Map 'Show All' and 'Hide All' commands
					FUIAction ShowAllFoliage(
						FExecuteAction::CreateSP(Viewport.ToSharedRef(), &::SLevelViewport::ToggleAllFoliageTypes, true)
					);
					FUIAction HideAllFoliage(
						FExecuteAction::CreateSP(Viewport.ToSharedRef(), &::SLevelViewport::ToggleAllFoliageTypes, false)
					);

					Section.AddMenuEntry(
						"ShowAll", LOCTEXT("ShowAllLabel", "Show All"), FText::GetEmpty(), FSlateIcon(), ShowAllFoliage
					);
					Section.AddMenuEntry(
						"HideAll", LOCTEXT("HideAllLabel", "Hide All"), FText::GetEmpty(), FSlateIcon(), HideAllFoliage
					);
				}

				// Gather all foliage types used in this world and group them by sub-levels
				auto AllFoliageMap =
					UE::LevelEditor::Private::GroupFoliageByOuter(GEditor->GetFoliageTypesInWorld(Viewport->GetWorld()));

				for (auto& FoliagePair : AllFoliageMap)
				{
					// Name foliage group by an outer sub-level name, or empty if foliage type is an asset
					FText EntryName =
						(FoliagePair.Key == NAME_None ? FText::GetEmpty()
													  : FText::FromName(FPackageName::GetShortFName(FoliagePair.Key)));
					FToolMenuSection& Section = Submenu->AddSection(NAME_None, EntryName);

					TArray<UFoliageType*>& FoliageList = FoliagePair.Value;
					for (UFoliageType* FoliageType : FoliageList)
					{
						FName MeshName = FoliageType->GetDisplayFName();
						TWeakObjectPtr<UFoliageType> FoliageTypePtr = FoliageType;

						FUIAction Action(
							FExecuteAction::CreateSP(
								Viewport.ToSharedRef(), &::SLevelViewport::ToggleShowFoliageType, FoliageTypePtr
							),
							FCanExecuteAction(),
							FIsActionChecked::CreateSP(Viewport.ToSharedRef(), &::SLevelViewport::IsFoliageTypeVisible, FoliageTypePtr)
						);

						Section.AddMenuEntry(
							NAME_None,
							FText::FromName(MeshName),
							FText::GetEmpty(),
							FSlateIcon(),
							Action,
							EUserInterfaceActionType::ToggleButton
						);
					}
				}
			}
		),
		false,
		FSlateIcon(FAppStyle::Get().GetStyleSetName(), "ShowFlagsMenu.SubMenu.FoliageTypes")
	);
}

FToolMenuEntry CreateShowHLODsSubmenu()
{
	// This is a dynamic entry so we can skip adding the submenu if the context
	// indicates that the viewport's world isn't partitioned.
	return FToolMenuEntry::InitDynamicEntry(
		"ShowHLODsDynamic",
		FNewToolMenuSectionDelegate::CreateLambda(
			[](FToolMenuSection& InDynamicSection) -> void
			{
				ULevelViewportContext* const LevelViewportContext = InDynamicSection.FindContext<ULevelViewportContext>();
				if (!LevelViewportContext)
				{
					return;
				}

				TSharedPtr<::SLevelViewport> Viewport = LevelViewportContext->LevelViewport.Pin();
				if (!Viewport)
				{
					return;
				}

				UWorld* World = Viewport->GetWorld();
				if (!World)
				{
					return;
				}

				// Only add this submenu for partitioned worlds.
				if (!World->IsPartitionedWorld())
				{
					return;
				}

				InDynamicSection.AddSubMenu(
					"ShowHLODsMenu",
					LOCTEXT("ShowHLODsMenu", "HLODs"),
					LOCTEXT("ShowHLODsMenu_ToolTip", "Settings for HLODs in editor"),
					FNewToolMenuDelegate::CreateLambda(
						[](UToolMenu* Submenu)
						{
							ULevelViewportContext* const LevelViewportContext =
								Submenu->FindContext<ULevelViewportContext>();
							if (!LevelViewportContext)
							{
								return;
							}

							TSharedPtr<::SLevelViewport> Viewport = LevelViewportContext->LevelViewport.Pin();
							if (!Viewport)
							{
								return;
							}

							UWorld* World = Viewport->GetWorld();
							UWorldPartition* WorldPartition = World ? World->GetWorldPartition() : nullptr;
							if (!WorldPartition)
							{
								return;
							}

							IWorldPartitionEditorModule* WorldPartitionEditorModule =
								FModuleManager::GetModulePtr<IWorldPartitionEditorModule>("WorldPartitionEditor");
							if (!WorldPartitionEditorModule)
							{
								return;
							}

							FText HLODInEditorDisallowedReason;
							const bool bHLODInEditorAllowed =
								WorldPartitionEditorModule->IsHLODInEditorAllowed(World, &HLODInEditorDisallowedReason);

							// Show HLODs
							{
								FToolUIAction UIAction;
								UIAction.ExecuteAction = FToolMenuExecuteAction::CreateLambda(
									[WorldPartitionEditorModule](const FToolMenuContext& InContext)
									{
										WorldPartitionEditorModule->SetShowHLODsInEditor(
											!WorldPartitionEditorModule->GetShowHLODsInEditor()
										);
									}
								);
								UIAction.CanExecuteAction = FToolMenuCanExecuteAction::CreateLambda(
									[bHLODInEditorAllowed](const FToolMenuContext& InContext)
									{
										return bHLODInEditorAllowed;
									}
								);
								UIAction.GetActionCheckState = FToolMenuGetActionCheckState::CreateLambda(
									[WorldPartitionEditorModule](const FToolMenuContext& InContext)
									{
										return WorldPartitionEditorModule->GetShowHLODsInEditor()
												 ? ECheckBoxState::Checked
												 : ECheckBoxState::Unchecked;
									}
								);
								FToolMenuEntry MenuEntry = FToolMenuEntry::InitMenuEntry(
									"ShowHLODs",
									LOCTEXT("ShowHLODs", "Show HLODs"),
									bHLODInEditorAllowed ? LOCTEXT("ShowHLODsToolTip", "Show/Hide HLODs")
														 : HLODInEditorDisallowedReason,
									FSlateIcon(),
									UIAction,
									EUserInterfaceActionType::ToggleButton
								);
								Submenu->AddMenuEntry(NAME_None, MenuEntry);
							}

							// Show HLODs Over Loaded Regions
							{
								FToolUIAction UIAction;
								UIAction.ExecuteAction = FToolMenuExecuteAction::CreateLambda(
									[WorldPartitionEditorModule](const FToolMenuContext& InContext)
									{
										WorldPartitionEditorModule->SetShowHLODsOverLoadedRegions(
											!WorldPartitionEditorModule->GetShowHLODsOverLoadedRegions()
										);
									}
								);
								UIAction.CanExecuteAction = FToolMenuCanExecuteAction::CreateLambda(
									[bHLODInEditorAllowed](const FToolMenuContext& InContext)
									{
										return bHLODInEditorAllowed;
									}
								);
								UIAction.GetActionCheckState = FToolMenuGetActionCheckState::CreateLambda(
									[WorldPartitionEditorModule](const FToolMenuContext& InContext)
									{
										return WorldPartitionEditorModule->GetShowHLODsOverLoadedRegions()
												 ? ECheckBoxState::Checked
												 : ECheckBoxState::Unchecked;
									}
								);
								FToolMenuEntry ShowHLODsEntry = FToolMenuEntry::InitMenuEntry(
									"ShowHLODsOverLoadedRegions",
									LOCTEXT("ShowHLODsOverLoadedRegions", "Show HLODs Over Loaded Regions"),
									bHLODInEditorAllowed
										? LOCTEXT("ShowHLODsOverLoadedRegions_ToolTip", "Show/Hide HLODs over loaded actors or regions")
										: HLODInEditorDisallowedReason,
									FSlateIcon(),
									UIAction,
									EUserInterfaceActionType::ToggleButton
								);
								Submenu->AddMenuEntry(NAME_None, ShowHLODsEntry);
							}

							// Min/Max Draw Distance
							{
								const double MinDrawDistanceMinValue = 0;
								const double MinDrawDistanceMaxValue = 102400;

								const double MaxDrawDistanceMinValue = 0;
								const double MaxDrawDistanceMaxValue = 1638400;

								// double SLevelViewportToolBar::OnGetHLODInEditorMinDrawDistanceValue() const
								auto OnGetHLODInEditorMinDrawDistanceValue = []() -> double
								{
									IWorldPartitionEditorModule* WorldPartitionEditorModule =
										FModuleManager::GetModulePtr<IWorldPartitionEditorModule>("WorldPartitionEditor");
									return WorldPartitionEditorModule
											 ? WorldPartitionEditorModule->GetHLODInEditorMinDrawDistance()
											 : 0;
								};

								// void SLevelViewportToolBar::OnHLODInEditorMinDrawDistanceValueChanged(double NewValue) const
								auto OnHLODInEditorMinDrawDistanceValueChanged = [](double NewValue) -> void
								{
									IWorldPartitionEditorModule* WorldPartitionEditorModule =
										FModuleManager::GetModulePtr<IWorldPartitionEditorModule>("WorldPartitionEditor");
									if (WorldPartitionEditorModule)
									{
										WorldPartitionEditorModule->SetHLODInEditorMinDrawDistance(NewValue);
										GEditor->RedrawLevelEditingViewports(true);
									}
								};

								TSharedRef<SSpinBox<double>> MinDrawDistanceSpinBox =
									SNew(SSpinBox<double>)
										.MinValue(MinDrawDistanceMinValue)
										.MaxValue(MinDrawDistanceMaxValue)
										.IsEnabled(bHLODInEditorAllowed)
										.Value_Lambda(OnGetHLODInEditorMinDrawDistanceValue)
										.OnValueChanged_Lambda(OnHLODInEditorMinDrawDistanceValueChanged)
										.ToolTipText(
											bHLODInEditorAllowed
												? LOCTEXT(
													  "HLODsInEditor_MinDrawDistance_Tooltip",
													  "Sets the minimum distance at which HLOD will be rendered"
												  )
												: HLODInEditorDisallowedReason
										)
										.OnBeginSliderMovement_Lambda(
											[]()
											{
												// Disable Slate throttling during slider drag to ensure immediate updates while moving the slider.
												FSlateThrottleManager::Get().DisableThrottle(true);
											}
										)
										.OnEndSliderMovement_Lambda(
											[](float)
											{
												FSlateThrottleManager::Get().DisableThrottle(false);
											}
										);

								TSharedRef<SSpinBox<double>> MaxDrawDistanceSpinBox =
									SNew(SSpinBox<double>)
										.MinValue(MaxDrawDistanceMinValue)
										.MaxValue(MaxDrawDistanceMaxValue)
										.IsEnabled(bHLODInEditorAllowed)
										.Value_Lambda(OnGetHLODInEditorMinDrawDistanceValue)
										.OnValueChanged_Lambda(OnHLODInEditorMinDrawDistanceValueChanged)
										.ToolTipText(
											bHLODInEditorAllowed
												? LOCTEXT(
													  "HLODsInEditor_MaxDrawDistance_Tooltip",
													  "Sets the maximum distance at which HLODs will be rendered"
												  )
												: HLODInEditorDisallowedReason
										)
										.OnBeginSliderMovement_Lambda(
											[]()
											{
												// Disable Slate throttling during slider drag to ensure immediate updates while moving the slider.
												FSlateThrottleManager::Get().DisableThrottle(true);
											}
										)
										.OnEndSliderMovement_Lambda(
											[](float)
											{
												FSlateThrottleManager::Get().DisableThrottle(false);
											}
										);

								auto CreateDrawDistanceWidget = [](TSharedRef<SSpinBox<double>> InSpinBoxWidget)
								{
									return SNew(SBox).HAlign(HAlign_Right
									)[SNew(SBox)
										  .Padding(FMargin(0.0f, 0.0f, 0.0f, 0.0f))
										  .WidthOverride(100.0f
										  )[SNew(SBorder)
												.BorderImage(FAppStyle::Get().GetBrush("Menu.WidgetBorder"))
												.Padding(FMargin(1.0f))[InSpinBoxWidget]]];
								};

								FToolMenuEntry MinDrawDistanceMenuEntry = FToolMenuEntry::InitWidget(
									"Min Draw Distance",
									CreateDrawDistanceWidget(MinDrawDistanceSpinBox),
									LOCTEXT("MinDrawDistance", "Min Draw Distance")
								);
								Submenu->AddMenuEntry(NAME_None, MinDrawDistanceMenuEntry);

								FToolMenuEntry MaxDrawDistanceMenuEntry = FToolMenuEntry::InitWidget(
									"Max Draw Distance",
									CreateDrawDistanceWidget(MaxDrawDistanceSpinBox),
									LOCTEXT("MaxDrawDistance", "Max Draw Distance")
								);
								Submenu->AddMenuEntry(NAME_None, MaxDrawDistanceMenuEntry);
							}
						}
					),
					false,
					FSlateIcon(FAppStyle::Get().GetStyleSetName(), "ShowFlagsMenu.SubMenu.HLODs")
				);
			}
		)
	);
}

FToolMenuEntry CreateShowLayersSubmenu()
{
	// This is a dynamic entry so we can skip adding the submenu if the context
	// indicates that the viewport's world is partitioned.
	return FToolMenuEntry::InitDynamicEntry(
		"ShowHLODsDynamic",
		FNewToolMenuSectionDelegate::CreateLambda(
			[](FToolMenuSection& InDynamicSection) -> void
			{
				ULevelViewportContext* const LevelViewportContext = InDynamicSection.FindContext<ULevelViewportContext>();
				if (!LevelViewportContext)
				{
					return;
				}

				TSharedPtr<::SLevelViewport> Viewport = LevelViewportContext->LevelViewport.Pin();
				if (!Viewport)
				{
					return;
				}

				UWorld* World = Viewport->GetWorld();
				if (!World)
				{
					return;
				}

				// Only add this submenu for non-partitioned worlds.
				if (World->IsPartitionedWorld())
				{
					return;
				}

				InDynamicSection.AddSubMenu(
					"ShowLayers",
					LOCTEXT("ShowLayersMenu", "Layers"),
					LOCTEXT("ShowLayersMenu_ToolTip", "Show layers flags"),
					FNewToolMenuDelegate::CreateStatic(
						&UE::LevelEditor::Private::PopulateShowLayersSubmenu, Viewport.ToWeakPtr()
					),
					false,
					FSlateIcon(FAppStyle::Get().GetStyleSetName(), "ShowFlagsMenu.SubMenu.Layers")
				);
			}
		)
	);
}

FToolMenuEntry CreateShowSpritesSubmenu()
{
	const FLevelViewportCommands& Actions = FLevelViewportCommands::Get();
	TArray<FLevelViewportCommands::FShowMenuCommand> ShowSpritesMenu;

	// 'Show All' and 'Hide All' buttons
	ShowSpritesMenu.Add(
		FLevelViewportCommands::FShowMenuCommand(Actions.ShowAllSprites, LOCTEXT("ShowAllLabel", "Show All"))
	);
	ShowSpritesMenu.Add(
		FLevelViewportCommands::FShowMenuCommand(Actions.HideAllSprites, LOCTEXT("HideAllLabel", "Hide All"))
	);

	// Get each show flag command and put them in their corresponding groups
	ShowSpritesMenu += Actions.ShowSpriteCommands;

	return FToolMenuEntry::InitSubMenu(
		"ShowSprites",
		LOCTEXT("ShowSpritesMenu", "Sprites"),
		LOCTEXT("ShowSpritesMenu_ToolTip", "Show sprites flags"),
		FNewToolMenuDelegate::CreateStatic(&UE::LevelEditor::Private::PopulateMenuWithCommands, ShowSpritesMenu, 2),
		false,
		FSlateIcon(FAppStyle::Get().GetStyleSetName(), "ShowFlagsMenu.SubMenu.Sprites")
	);
}

FToolMenuEntry CreateShowVolumesSubmenu()
{
	const FLevelViewportCommands& Actions = FLevelViewportCommands::Get();
	TArray<FLevelViewportCommands::FShowMenuCommand> ShowVolumesMenu;

	// 'Show All' and 'Hide All' buttons
	ShowVolumesMenu.Add(
		FLevelViewportCommands::FShowMenuCommand(Actions.ShowAllVolumes, LOCTEXT("ShowAllLabel", "Show All"))
	);
	ShowVolumesMenu.Add(
		FLevelViewportCommands::FShowMenuCommand(Actions.HideAllVolumes, LOCTEXT("HideAllLabel", "Hide All"))
	);

	// Get each show flag command and put them in their corresponding groups
	ShowVolumesMenu += Actions.ShowVolumeCommands;

	return FToolMenuEntry::InitSubMenu(
		"ShowVolumes",
		LOCTEXT("ShowVolumesMenu", "Volumes"),
		LOCTEXT("ShowVolumesMenu_ToolTip", "Show volumes flags"),
		FNewToolMenuDelegate::CreateStatic(&UE::LevelEditor::Private::PopulateMenuWithCommands, ShowVolumesMenu, 2),
		false,
		FSlateIcon(FAppStyle::Get().GetStyleSetName(), "ShowFlagsMenu.SubMenu.Volumes")
	);
}

#if STATS
FToolMenuEntry CreateShowStatsSubmenu()
{
	return FToolMenuEntry::InitSubMenu(
		"ShowStatsMenu",
		LOCTEXT("ShowStatsMenu", "Stat"),
		LOCTEXT("ShowStatsMenu_ToolTip", "Show Stat commands"),
		FNewToolMenuDelegate::CreateLambda(
			[](UToolMenu* InMenu) -> void
			{
				TArray<FLevelViewportCommands::FShowMenuCommand> HideStatsMenu;
				HideStatsMenu.Add(FLevelViewportCommands::FShowMenuCommand(
					FLevelViewportCommands::Get().HideAllStats, LOCTEXT("HideAllLabel", "Hide All")
				));

				UE::LevelEditor::Private::PopulateMenuWithCommands(InMenu, HideStatsMenu, 1);

				FToolMenuSection& Section = InMenu->FindOrAddSection("Section");

				// Separate out stats into two list, those with and without submenus
				TArray<FLevelViewportCommands::FShowMenuCommand> SingleStatCommands;
				TMap<FString, TArray<FLevelViewportCommands::FShowMenuCommand>> SubbedStatCommands;
				for (auto StatCatIt = FLevelViewportCommands::Get().ShowStatCatCommands.CreateConstIterator(); StatCatIt;
					 ++StatCatIt)
				{
					const TArray<FLevelViewportCommands::FShowMenuCommand>& ShowStatCommands = StatCatIt.Value();
					const FString& CategoryName = StatCatIt.Key();

					// If no category is specified, or there's only one category, don't use submenus
					FString NoCategory = FStatConstants::NAME_NoCategory.ToString();
					NoCategory.RemoveFromStart(TEXT("STATCAT_"));
					if (CategoryName == NoCategory || FLevelViewportCommands::Get().ShowStatCatCommands.Num() == 1)
					{
						for (int32 StatIndex = 0; StatIndex < ShowStatCommands.Num(); ++StatIndex)
						{
							const FLevelViewportCommands::FShowMenuCommand& StatCommand = ShowStatCommands[StatIndex];
							SingleStatCommands.Add(StatCommand);
						}
					}
					else
					{
						SubbedStatCommands.Add(CategoryName, ShowStatCommands);
					}
				}

				// First add all the stats that don't have a sub menu
				for (auto StatCatIt = SingleStatCommands.CreateConstIterator(); StatCatIt; ++StatCatIt)
				{
					const FLevelViewportCommands::FShowMenuCommand& StatCommand = *StatCatIt;
					Section.AddMenuEntry(NAME_None, StatCommand.ShowMenuItem, StatCommand.LabelOverride);
				}

				// Now add all the stats that have sub menus
				for (auto StatCatIt = SubbedStatCommands.CreateConstIterator(); StatCatIt; ++StatCatIt)
				{
					const TArray<FLevelViewportCommands::FShowMenuCommand>& StatCommands = StatCatIt.Value();
					const FText CategoryName = FText::FromString(StatCatIt.Key());

					FFormatNamedArguments Args;
					Args.Add(TEXT("StatCat"), CategoryName);
					const FText CategoryDescription =
						FText::Format(NSLOCTEXT("UICommands", "StatShowCatName", "Show {StatCat} stats"), Args);

					Section.AddSubMenu(
						NAME_None,
						CategoryName,
						CategoryDescription,
						FNewToolMenuDelegate::CreateStatic(&UE::LevelEditor::Private::PopulateMenuWithCommands, StatCommands, 0)
					);
				}
			}
		),
		false,
		FSlateIcon(FAppStyle::Get().GetStyleSetName(), "EditorViewport.SubMenu.Stats")
	);
}
#endif

FToolMenuEntry CreateViewportToolbarShowSubmenu()
{
	return FToolMenuEntry::InitSubMenu(
		"Show",
		LOCTEXT("ShowSubmenuLabel", "Show"),
		LOCTEXT("ShowSubmenuTooltip", "Show flags related to the current viewport"),
		FNewToolMenuDelegate::CreateLambda(
			[](UToolMenu* InMenu) -> void
			{
				{
					FToolMenuSection& UnnamedSection = InMenu->FindOrAddSection(NAME_None);

					UnnamedSection.AddMenuEntry(FLevelViewportCommands::Get().UseDefaultShowFlags);

					UnnamedSection.AddSeparator("ViewportStatsSeparator");

#if STATS
					// Override the label of the stats submenu for the new viewport toolbar.
					{
						FToolMenuEntry StatsSubmenu = UE::LevelEditor::CreateShowStatsSubmenu();
						StatsSubmenu.Label = LOCTEXT("ViewportStatsLabel", "Viewport Stats");
						UnnamedSection.AddEntry(StatsSubmenu);
					}
#endif
				}

				{
					FToolMenuSection& CommonShowFlagsSection =
						InMenu->FindOrAddSection("CommonShowFlags", LOCTEXT("CommonShowFlagsLabel", "Common Show Flags"));

					FShowFlagMenuCommands::Get().PopulateCommonShowFlagsSection(CommonShowFlagsSection);
				}

				{
					FToolMenuSection& AllShowFlagsSection =
						InMenu->FindOrAddSection("AllShowFlags", LOCTEXT("AllShowFlagsLabel", "All Show Flags"));

					{
						FToolMenuEntry ShowFoliageSubmenu = CreateShowFoliageSubmenu();
						ShowFoliageSubmenu.Label = LOCTEXT("ShowFoliageLabel", "Foliage");
						AllShowFlagsSection.AddEntry(ShowFoliageSubmenu);
					}

					AllShowFlagsSection.AddEntry(CreateShowHLODsSubmenu());
					AllShowFlagsSection.AddEntry(CreateShowLayersSubmenu());
					AllShowFlagsSection.AddEntry(CreateShowSpritesSubmenu());
					AllShowFlagsSection.AddEntry(CreateShowVolumesSubmenu());

					FShowFlagMenuCommands::Get().PopulateAllShowFlagsSection(AllShowFlagsSection);
				}

				// Create these sections for backward compatibility with the old viewport toolbar.
				{
					// If your entries end up in this section, you should move it to the new "CommonShowFlags" section instead.
					InMenu->FindOrAddSection(
						"ShowFlagsMenuSectionCommon",
						LOCTEXT("ShowFlagsMenuSectionCommonLabel", "Common Show Flags (Deprecated section)")
					);

					// If your entries end up in these sections, you should move them to the above "AllShowFlags" section instead.
					InMenu->FindOrAddSection(
						"LevelViewportShowFlags",
						LOCTEXT("LevelViewportShowFlagsLabel", "All Show Flags (Deprecated section)")
					);
					InMenu->FindOrAddSection(
						"LevelViewportEditorShow", LOCTEXT("LevelViewportEditorShowLabel", "Editor (Deprecated section)")
					);
				}
			}
		)
	);
}

FToolMenuEntry CreateFeatureLevelPreviewSubmenu()
{
	return FToolMenuEntry::InitSubMenu(
		"FeatureLevelPreview",
		NSLOCTEXT("LevelToolBarViewMenu", "PreviewPlatformSubMenu", "Preview Platform"),
		NSLOCTEXT("LevelToolBarViewMenu", "PreviewPlatformSubMenu_ToolTip", "Sets the preview platform used by the main editor"),
		FNewToolMenuDelegate::CreateLambda(
			[](UToolMenu* InMenu) -> void
			{
				FToolMenuSection& Section =
					InMenu->AddSection("EditorPreviewMode", LOCTEXT("EditorPreviewModeDevices", "Preview Devices"));
				// Preview platforms discovered from ITargetPlatforms.
				for (auto& Item : FLevelEditorCommands::Get().PreviewPlatformOverrides)
				{
					Section.AddMenuEntry(Item);
				}
			}
		)
	);
}

FToolMenuEntry CreateMaterialQualityLevelSubmenu()
{
	return FToolMenuEntry::InitSubMenu(
		"MaterialQualityLevel",
		NSLOCTEXT("LevelToolBarViewMenu", "MaterialQualityLevelSubMenu", "Material Quality Level"),
		NSLOCTEXT(
			"LevelToolBarViewMenu",
			"MaterialQualityLevelSubMenu_ToolTip",
			"Sets the value of the CVar \"r.MaterialQualityLevel\" (low=0, high=1, medium=2, Epic=3). This affects "
			"materials via the QualitySwitch material expression."
		),
		FNewToolMenuDelegate::CreateLambda(
			[](UToolMenu* InMenu) -> void
			{
				FToolMenuSection& Section = InMenu->AddSection(
					"LevelEditorMaterialQualityLevel",
					NSLOCTEXT("LevelToolBarViewMenu", "MaterialQualityLevelHeading", "Material Quality Level")
				);
				Section.AddMenuEntry(FLevelEditorCommands::Get().MaterialQualityLevel_Low);
				Section.AddMenuEntry(FLevelEditorCommands::Get().MaterialQualityLevel_Medium);
				Section.AddMenuEntry(FLevelEditorCommands::Get().MaterialQualityLevel_High);
				Section.AddMenuEntry(FLevelEditorCommands::Get().MaterialQualityLevel_Epic);
			}
		)
	);
}

FToolMenuEntry CreateViewportToolbarPerformanceAndScalabilitySubmenu()
{
	return FToolMenuEntry::InitSubMenu(
		"PerformanceAndScalability",
		LOCTEXT("PerformanceAndScalabilityLabel", "Performance & Scalability"),
		LOCTEXT("PerformanceAndScalabilityTooltip", "Performance and scalability tools tied to this viewport."),
		FNewToolMenuDelegate::CreateLambda(
			[](UToolMenu* Submenu) -> void
			{
				{
					FToolMenuSection& UnnamedSection = Submenu->FindOrAddSection(NAME_None);

					// Add realtime rendering toggle.
					UnnamedSection.AddDynamicEntry(
						"ToggleRealtimeDynamicSection",
						FNewToolMenuSectionDelegate::CreateLambda(
							[](FToolMenuSection& InnerSection) -> void
							{
								FToolUIAction RealtimeToggleAction;
								RealtimeToggleAction.ExecuteAction = FToolMenuExecuteAction::CreateLambda(
									[](const FToolMenuContext& Context) -> void
									{
										ULevelViewportContext* const LevelViewportContext =
											Context.FindContext<ULevelViewportContext>();
										if (!LevelViewportContext)
										{
											return;
										}

										if (const TSharedPtr<::SLevelViewport> LevelViewport =
												LevelViewportContext->LevelViewport.Pin())
										{
											LevelViewport->OnToggleRealtime();
										}
									}
								);

								RealtimeToggleAction.GetActionCheckState = FToolMenuGetActionCheckState::CreateLambda(
									[](const FToolMenuContext& Context) -> ECheckBoxState
									{
										ULevelViewportContext* const LevelViewportContext =
											Context.FindContext<ULevelViewportContext>();
										if (!LevelViewportContext)
										{
											return ECheckBoxState::Undetermined;
										}

										if (const TSharedPtr<::SLevelViewport> LevelViewport =
												LevelViewportContext->LevelViewport.Pin())
										{
											return LevelViewport->IsRealtime() ? ECheckBoxState::Checked
																			   : ECheckBoxState::Unchecked;
										}

										return ECheckBoxState::Undetermined;
									}
								);

								TAttribute<FText> Tooltip;
								{
									const FText NonRealtimeTooltip = LOCTEXT(
										"ToggleRealtimeTooltip_WarnRealtimeOff",
										"This viewport is not updating in realtime.  Click to turn on realtime mode."
									);
									const FText RealtimeTooltip =
										LOCTEXT("ToggleRealtimeTooltip", "Toggle realtime rendering of the viewport");

									// If we can find a context with a viewport, use that to adjust the tooltip
									// based on the viewport's realtime status.
									if (ULevelViewportContext* const LevelViewportContext =
											InnerSection.FindContext<ULevelViewportContext>())
									{
										Tooltip = TAttribute<FText>::CreateLambda(
											[WeakViewport = LevelViewportContext->LevelViewport,
											 NonRealtimeTooltip,
											 RealtimeTooltip]() -> FText
											{
												bool bDisplayTopLevel = false;
												if (const TSharedPtr<::SLevelViewport> LevelViewport = WeakViewport.Pin())
												{
													bDisplayTopLevel = !LevelViewport->IsRealtime();
												}

												return bDisplayTopLevel ? NonRealtimeTooltip : RealtimeTooltip;
											}
										);
									}
									else
									{
										Tooltip = RealtimeTooltip;
									}
								}

								FToolMenuEntry ToggleRealtime = FToolMenuEntry::InitMenuEntry(
									"ToggleRealtime",
									LOCTEXT("ToggleRealtimeLabel", "Realtime Viewport"),
									Tooltip,
									FSlateIcon(FAppStyle::GetAppStyleSetName(), "EditorViewport.ToggleRealTime"),
									RealtimeToggleAction,
									EUserInterfaceActionType::ToggleButton
								);

								// If we can find a context with a viewport, bind the top-level status of the
								// realtime button to the viewport's realtime state where we show the realtime
								// toggle in the top-level if the viewport is NOT realtime.
								if (ULevelViewportContext* const LevelViewportContext =
										InnerSection.FindContext<ULevelViewportContext>())
								{
									ToggleRealtime.SetShowInToolbarTopLevel(TAttribute<bool>::CreateLambda(
										[WeakViewport = LevelViewportContext->LevelViewport]() -> bool
										{
											if (const TSharedPtr<::SLevelViewport> LevelViewport = WeakViewport.Pin())
											{
												return !LevelViewport->IsRealtime();
											}
											return false;
										}
									));
								}

								InnerSection.AddEntry(ToggleRealtime);
							}
						)
					);
				}

				{
					FToolMenuSection& PerformanceAndScalabilitySection = Submenu->FindOrAddSection(
						"PerformanceAndScalability",
						LOCTEXT("PerformanceAndScalabilitySectionLabel", "Performance & Scalability")
					);

					PerformanceAndScalabilitySection.AddEntry(CreateFeatureLevelPreviewSubmenu());

					PerformanceAndScalabilitySection.AddSeparator("PerformanceAndScalabilitySettings");

					PerformanceAndScalabilitySection.AddSubMenu(
						"Scalability",
						LOCTEXT("ScalabilitySubMenu", "Engine Scalability"),
						LOCTEXT("ScalabilitySubMenu_ToolTip", "Open the engine scalability settings"),
						FNewToolMenuDelegate::CreateLambda(
							[](UToolMenu* InMenu) -> void
							{
								FToolMenuSection& Section = InMenu->FindOrAddSection(NAME_None);
								Section.AddEntry(FToolMenuEntry::InitWidget(
									"ScalabilitySettings", SNew(SScalabilitySettings), FText(), true
								));
							}
						)
					);

					PerformanceAndScalabilitySection.AddEntry(CreateMaterialQualityLevelSubmenu());

					// FEditorViewportClient& ViewportClient = Viewport.Pin()->GetLevelViewportClient();
					PerformanceAndScalabilitySection.AddSubMenu(
						"ScreenPercentageSubMenu",
						LOCTEXT("ScreenPercentageSubMenu", "Screen Percentage"),
						LOCTEXT("ScreenPercentageSubMenu_ToolTip", "Customize the viewport's screen percentage"),
						FNewToolMenuDelegate::CreateLambda(
							[](UToolMenu* ScreenPercentageSubMenu)
							{
								FToolMenuSection& UnnamedSection = ScreenPercentageSubMenu->FindOrAddSection(NAME_None);

								ScreenPercentageSubMenu->AddDynamicSection(
									NAME_None,
									FNewToolMenuDelegateLegacy::CreateLambda(
										[](FMenuBuilder& MenuBuilder, UToolMenu* InMenu) -> void
										{
											ULevelViewportContext* const LevelViewportContext =
												InMenu->FindContext<ULevelViewportContext>();
											if (!LevelViewportContext)
											{
												return;
											}

											if (const TSharedPtr<::SLevelViewport> LevelViewport =
													LevelViewportContext->LevelViewport.Pin())
											{
												TSharedPtr<FEditorViewportClient> Client =
													LevelViewport->GetViewportClient();
												SCommonEditorViewportToolbarBase::ConstructScreenPercentageMenu(
													MenuBuilder, Client.Get()
												);
											}
										}
									)
								);
							}
						)
					);
				}
			}
		)
	);
}

void GenerateViewportLayoutsMenu(UToolMenu* InMenu, TSharedPtr<::SLevelViewport> InViewport)
{
	TSharedPtr<FUICommandList> CommandList = InViewport->GetCommandList();

	// Disable searching in this menu because it only contains visual representations of
	// viewport layouts without any searchable text.
	InMenu->bSearchable = false;

	{
		FToolMenuSection& Section =
			InMenu->AddSection("LevelViewportOnePaneConfigs", LOCTEXT("OnePaneConfigHeader", "One Pane"));

		FSlimHorizontalToolBarBuilder OnePaneButton(CommandList, FMultiBoxCustomization::None);
		OnePaneButton.SetLabelVisibility(EVisibility::Collapsed);
		OnePaneButton.SetStyle(&FAppStyle::Get(), "ViewportLayoutToolbar");

		OnePaneButton.AddToolBarButton(FLevelViewportCommands::Get().ViewportConfig_OnePane);

		Section.AddEntry(FToolMenuEntry::InitWidget(
			"LevelViewportOnePaneConfigs",
			// clang-format off
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				OnePaneButton.MakeWidget()
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1)
			[
				SNullWidget::NullWidget
			],
			// clang-format on
			FText::GetEmpty(),
			true
		));
	}

	{
		FToolMenuSection& Section =
			InMenu->AddSection("LevelViewportTwoPaneConfigs", LOCTEXT("TwoPaneConfigHeader", "Two Panes"));
		FSlimHorizontalToolBarBuilder TwoPaneButtons(CommandList, FMultiBoxCustomization::None);
		TwoPaneButtons.SetLabelVisibility(EVisibility::Collapsed);
		TwoPaneButtons.SetStyle(&FAppStyle::Get(), "ViewportLayoutToolbar");

		TwoPaneButtons.AddToolBarButton(FLevelViewportCommands::Get().ViewportConfig_TwoPanesH, NAME_None, FText());
		TwoPaneButtons.AddToolBarButton(FLevelViewportCommands::Get().ViewportConfig_TwoPanesV, NAME_None, FText());

		Section.AddEntry(FToolMenuEntry::InitWidget(
			"LevelViewportTwoPaneConfigs",
			// clang-format off
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				TwoPaneButtons.MakeWidget()
			]
			+SHorizontalBox::Slot()
			.FillWidth(1)
			[
				SNullWidget::NullWidget
			],
			// clang-format on
			FText::GetEmpty(),
			true
		));
	}

	{
		FToolMenuSection& Section =
			InMenu->AddSection("LevelViewportThreePaneConfigs", LOCTEXT("ThreePaneConfigHeader", "Three Panes"));
		FSlimHorizontalToolBarBuilder ThreePaneButtons(CommandList, FMultiBoxCustomization::None);
		ThreePaneButtons.SetLabelVisibility(EVisibility::Collapsed);
		ThreePaneButtons.SetStyle(&FAppStyle::Get(), "ViewportLayoutToolbar");

		ThreePaneButtons.AddToolBarButton(FLevelViewportCommands::Get().ViewportConfig_ThreePanesLeft, NAME_None, FText());
		ThreePaneButtons.AddToolBarButton(FLevelViewportCommands::Get().ViewportConfig_ThreePanesRight, NAME_None, FText());
		ThreePaneButtons.AddToolBarButton(FLevelViewportCommands::Get().ViewportConfig_ThreePanesTop, NAME_None, FText());
		ThreePaneButtons.AddToolBarButton(FLevelViewportCommands::Get().ViewportConfig_ThreePanesBottom, NAME_None, FText());

		Section.AddEntry(FToolMenuEntry::InitWidget(
			"LevelViewportThreePaneConfigs",
			// clang-format off
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				ThreePaneButtons.MakeWidget()
			]
			+SHorizontalBox::Slot()
			.FillWidth(1)
			[
				SNullWidget::NullWidget
			],
			// clang-format on
			FText::GetEmpty(),
			true
		));
	}

	{
		FToolMenuSection& Section =
			InMenu->AddSection("LevelViewportFourPaneConfigs", LOCTEXT("FourPaneConfigHeader", "Four Panes"));
		FSlimHorizontalToolBarBuilder FourPaneButtons(CommandList, FMultiBoxCustomization::None);
		FourPaneButtons.SetLabelVisibility(EVisibility::Collapsed);
		FourPaneButtons.SetStyle(&FAppStyle::Get(), "ViewportLayoutToolbar");

		FourPaneButtons.AddToolBarButton(FLevelViewportCommands::Get().ViewportConfig_FourPanes2x2, NAME_None, FText());
		FourPaneButtons.AddToolBarButton(FLevelViewportCommands::Get().ViewportConfig_FourPanesLeft, NAME_None, FText());
		FourPaneButtons.AddToolBarButton(FLevelViewportCommands::Get().ViewportConfig_FourPanesRight, NAME_None, FText());
		FourPaneButtons.AddToolBarButton(FLevelViewportCommands::Get().ViewportConfig_FourPanesTop, NAME_None, FText());
		FourPaneButtons.AddToolBarButton(FLevelViewportCommands::Get().ViewportConfig_FourPanesBottom, NAME_None, FText());

		Section.AddEntry(FToolMenuEntry::InitWidget(
			"LevelViewportFourPaneConfigs",
			// clang-format off
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				FourPaneButtons.MakeWidget()
			]
			+SHorizontalBox::Slot()
			.FillWidth(1)
			[
				SNullWidget::NullWidget
			],
			// clang-format on
			FText::GetEmpty(),
			true
		));
	}
}

TSharedRef<SWidget> BuildVolumeControlCustomWidget()
{
	// clang-format off
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.FillWidth(0.9f)
		.Padding(FMargin(2.0f, 0.0f, 0.0f, 0.0f))
		[
			SNew(SVolumeControl)
				.ToolTipText_Static(&FLevelEditorActionCallbacks::GetAudioVolumeToolTip)
				.Volume_Static(&FLevelEditorActionCallbacks::GetAudioVolume)
				.OnVolumeChanged_Static(&FLevelEditorActionCallbacks::OnAudioVolumeChanged)
				.Muted_Static(&FLevelEditorActionCallbacks::GetAudioMuted)
				.OnMuteChanged_Static(&FLevelEditorActionCallbacks::OnAudioMutedChanged)
		]
		+ SHorizontalBox::Slot()
		.FillWidth(0.1f);
	// clang-format on
}

FToolMenuEntry CreateLevelEditorViewportToolbarSettingsSubmenu()
{
	return FToolMenuEntry::InitSubMenu(
		"Settings",
		LOCTEXT("SettingsSubmenuLabel", "Settings"),
		LOCTEXT("SettingsSubmenuTooltip", "Viewport-related settings"),
		FNewToolMenuDelegate::CreateLambda(
			[](UToolMenu* Submenu) -> void
			{
				{
					FToolMenuSection& ViewportControlsSection = Submenu->FindOrAddSection(
						"ViewportControls", LOCTEXT("ViewportControlsSectionLabel", "Viewport Controls")
					);

					ViewportControlsSection.AddSubMenu(
						"ViewportLayouts",
						LOCTEXT("ViewportLayoutsLabel", "Layouts"),
						LOCTEXT("ViewportLayoutsTooltip", "Configure the layouts of the viewport windows"),
						FNewToolMenuDelegate::CreateLambda(
							[](UToolMenu* InMenu)
							{
								ULevelViewportContext* const LevelViewportContext =
									InMenu->FindContext<ULevelViewportContext>();
								if (!LevelViewportContext)
								{
									return;
								}

								if (const TSharedPtr<::SLevelViewport> LevelViewport =
										LevelViewportContext->LevelViewport.Pin())
								{
									GenerateViewportLayoutsMenu(InMenu, LevelViewport);
								}
							}
						),
						false,
						FSlateIcon(FAppStyle::Get().GetStyleSetName(), "Icons.Layout")
					);
				}

				{
					FToolMenuSection& SettingsSection =
						Submenu->FindOrAddSection("Settings", LOCTEXT("SettingsSectionLabel", "Settings"));

					SettingsSection.AddEntry(FToolMenuEntry::InitWidget(
						"Volume", BuildVolumeControlCustomWidget(), LOCTEXT("VolumeControlLabel", "Volume")
					));

					SettingsSection.AddSeparator("ViewportSizeSeparator");

					SettingsSection.AddMenuEntry(FLevelViewportCommands::Get().ToggleImmersive);

					{
						FToolUIAction MaximizeRestoreAction;
						MaximizeRestoreAction.ExecuteAction = FToolMenuExecuteAction::CreateLambda(
							[](const FToolMenuContext& Context)
							{
								ULevelViewportContext* const LevelViewportContext =
									Context.FindContext<ULevelViewportContext>();
								if (!LevelViewportContext)
								{
									return;
								}

								if (const TSharedPtr<::SLevelViewport> LevelViewport =
										LevelViewportContext->LevelViewport.Pin())
								{
									LevelViewport->OnToggleMaximize();
								}
							}
						);
						MaximizeRestoreAction.GetActionCheckState = FToolMenuGetActionCheckState::CreateLambda(
							[](const FToolMenuContext& Context) -> ECheckBoxState
							{
								ULevelViewportContext* const LevelViewportContext =
									Context.FindContext<ULevelViewportContext>();
								if (!LevelViewportContext)
								{
									return ECheckBoxState::Undetermined;
								}

								if (const TSharedPtr<::SLevelViewport> LevelViewport =
										LevelViewportContext->LevelViewport.Pin())
								{
									return LevelViewport->IsMaximized() ? ECheckBoxState::Checked
																		: ECheckBoxState::Unchecked;
								}

								return ECheckBoxState::Undetermined;
							}
						);

						SettingsSection
							.AddMenuEntry(
								"MaximizeRestore",
								LOCTEXT("MaximizeRestoreLabel", "Maximize Viewport"),
								LOCTEXT("MaximizeRestoreTooltip", "Maximizes or restores this viewport"),
								FSlateIcon(FAppStyle::GetAppStyleSetName(), "EditorViewportToolBar.Maximize.Normal"),
								MaximizeRestoreAction,
								EUserInterfaceActionType::ToggleButton
							)
							.SetShowInToolbarTopLevel(true);
					}

					SettingsSection.AddSeparator("AdvancedSeparator");

					{
						const FLevelViewportCommands& LevelViewportActions = FLevelViewportCommands::Get();
						SettingsSection.AddMenuEntry(LevelViewportActions.AdvancedSettings);
					}
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

				SnappingSection.AddEntry(Private::CreateSurfaceSnapMenu());

				FToolMenuEntry GridSnapping = FToolMenuEntry::InitMenuEntry(FEditorViewportCommands::Get().LocationGridSnap);
				GridSnapping.UserInterfaceActionType = EUserInterfaceActionType::ToggleButton;
				GridSnapping.Label = LOCTEXT("GridSnapLabel", "Grid");
				GridSnapping.SetShowInToolbarTopLevel(true);
				SnappingSection.AddEntry(GridSnapping);

				FToolMenuEntry RotationSnapping = FToolMenuEntry::InitMenuEntry(FEditorViewportCommands::Get().RotationGridSnap);
				RotationSnapping.UserInterfaceActionType = EUserInterfaceActionType::ToggleButton;
				RotationSnapping.Label = LOCTEXT("RotationSnapLabel", "Rotation");
				SnappingSection.AddEntry(RotationSnapping);

				FToolMenuEntry ScaleSnapping = FToolMenuEntry::InitMenuEntry(FEditorViewportCommands::Get().ScaleGridSnap);
				ScaleSnapping.UserInterfaceActionType = EUserInterfaceActionType::ToggleButton;
				ScaleSnapping.Label = LOCTEXT("ScaleSnapLabel", "Scale");
				SnappingSection.AddEntry(ScaleSnapping);

				SnappingSection.AddEntry(Private::CreateActorSnapMenu());

				FToolMenuEntry SocketSnapping = FToolMenuEntry::InitMenuEntry(FLevelEditorCommands::Get().ToggleSocketSnapping);
				SocketSnapping.UserInterfaceActionType = EUserInterfaceActionType::ToggleButton;
				SocketSnapping.Label = LOCTEXT("SocketSnapLabel", "Socket");
				SnappingSection.AddEntry(SocketSnapping);

				FToolMenuEntry VertexSnapping = FToolMenuEntry::InitMenuEntry(FLevelEditorCommands::Get().EnableVertexSnap);
				VertexSnapping.UserInterfaceActionType = EUserInterfaceActionType::ToggleButton;
				VertexSnapping.Label = LOCTEXT("VertexSnapLabel", "Vertex");
				SnappingSection.AddEntry(VertexSnapping);

				// TODO: add Planar Snapping
			}
		)
	);
}

} // namespace UE::LevelEditor

#undef LOCTEXT_NAMESPACE
