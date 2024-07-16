// Copyright Epic Games, Inc. All Rights Reserved.

#include "ViewportToolbar/LevelEditorViewportToolbarSections.h"

#include "Bookmarks/BookmarkUI.h"
#include "Bookmarks/IBookmarkTypeTools.h"
#include "Camera/CameraActor.h"
#include "EditorViewportCommands.h"
#include "FoliageType.h"
#include "GameFramework/ActorPrimitiveColorHandler.h"
#include "GameFramework/WorldSettings.h"
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

void SetLevelViewportFOV(const TSharedRef<::SLevelViewport>& InLevelViewport, float InValue)
{
	bool bUpdateStoredFOV = true;

	if (InLevelViewport->GetLevelViewportClient().GetActiveActorLock().IsValid())
	{
		if (ACameraActor* CameraActor =
				Cast<ACameraActor>(InLevelViewport->GetLevelViewportClient().GetActiveActorLock().Get()))
		{
			CameraActor->GetCameraComponent()->FieldOfView = InValue;
			bUpdateStoredFOV = false;
		}
	}

	if (bUpdateStoredFOV)
	{
		InLevelViewport->GetLevelViewportClient().FOVAngle = InValue;
	}

	InLevelViewport->GetLevelViewportClient().ViewFOV = InValue;
	InLevelViewport->GetLevelViewportClient().Invalidate();
}

void SetFarViewPlaneValue(const TSharedRef<::SLevelViewport>& InLevelViewport, float InValue)
{
	FLevelEditorViewportClient& ViewportClient = InLevelViewport->GetLevelViewportClient();
	return ViewportClient.OverrideFarClipPlane(InValue);
}

float GetLevelViewportFOV(const TSharedRef<::SLevelViewport>& InLevelViewport)
{
	FLevelEditorViewportClient& ViewportClient = InLevelViewport->GetLevelViewportClient();
	return ViewportClient.ViewFOV;
}

float GetFarViewPlaneValue(const TSharedRef<::SLevelViewport>& InLevelViewport)
{
	FLevelEditorViewportClient& ViewportClient = InLevelViewport->GetLevelViewportClient();
	return ViewportClient.GetFarClipPlaneOverride();
}

// TODO: properly implement
void SetCameraSpeed(const TSharedRef<::SLevelViewport>& InLevelViewport, float NewValue)
{
}

void SetCameraSpeedScalarValue(const TSharedRef<::SLevelViewport>& InLevelViewport, float NewValue)
{
	if (InLevelViewport->GetViewportClient().IsValid())
	{
		InLevelViewport->GetViewportClient()->SetCameraSpeedScalar(NewValue);

		// TODO: make sure something like this gets called if needed (e.g. future menus sharing code)
		// Also, verify where/how to deal with this callback
		// OnCamSpeedScalarChanged.ExecuteIfBound(NewValue);
	}
}

// TODO: properly implement
float GetCamSpeedSliderPosition(const TSharedRef<::SLevelViewport>& InLevelViewport)
{
	return 1.0f;
}

float GetCamSpeedScalarSliderPosition(const TSharedRef<::SLevelViewport>& InLevelViewport)
{
	float CamSpeedScalar = 1.0f;

	if (InLevelViewport->GetViewportClient().IsValid())
	{
		CamSpeedScalar = (InLevelViewport->GetViewportClient()->GetCameraSpeedScalar());
	}

	return CamSpeedScalar;
}

bool AddJumpToBookmarkMenu(UToolMenu* InMenu, const TWeakPtr<::SLevelViewport>& InViewport)
{
	FToolMenuSection& Section =
		InMenu->FindOrAddSection("JumpToBookmark", LOCTEXT("JumpToBookmarksSectionName", "Jump to Bookmark"));

	// Add a menu entry for each bookmark
	TSharedPtr<::SLevelViewport> SharedViewport = InViewport.Pin();
	FLevelEditorViewportClient& ViewportClient = SharedViewport->GetLevelViewportClient();

	const int32 NumberOfBookmarks = static_cast<int32>(IBookmarkTypeTools::Get().GetMaxNumberOfBookmarks(&ViewportClient));
	const int32 NumberOfMappedBookmarks = FMath::Min<int32>(AWorldSettings::NumMappedBookmarks, NumberOfBookmarks);

	bool bFoundAnyBookmarks = false;

	for (int32 BookmarkIndex = 0; BookmarkIndex < NumberOfMappedBookmarks; ++BookmarkIndex)
	{
		if (IBookmarkTypeTools::Get().CheckBookmark(BookmarkIndex, &ViewportClient))
		{
			bFoundAnyBookmarks = true;
			Section.AddMenuEntry(
				NAME_None,
				FLevelViewportCommands::Get().JumpToBookmarkCommands[BookmarkIndex],
				FBookmarkUI::GetPlainLabel(BookmarkIndex),
				FBookmarkUI::GetJumpToTooltip(BookmarkIndex),
				FSlateIcon(FAppStyle::Get().GetStyleSetName(), "EditorViewport.SubMenu.Bookmarks")
			);
		}
	}

	return bFoundAnyBookmarks;
}

void AddClearBookmarkMenu(UToolMenu* InMenu, const TWeakPtr<::SLevelViewport>& InViewport)
{
	FToolMenuSection& Section = InMenu->AddSection("Section");

	// Add a menu entry for each bookmark
	// FEditorModeTools& Tools = GLevelEditorModeTools();
	TSharedPtr<::SLevelViewport> SharedViewport = InViewport.Pin();
	FLevelEditorViewportClient& ViewportClient = SharedViewport->GetLevelViewportClient();

	const int32 NumberOfBookmarks = static_cast<int32>(IBookmarkTypeTools::Get().GetMaxNumberOfBookmarks(&ViewportClient));
	const int32 NumberOfMappedBookmarks = FMath::Min<int32>(AWorldSettings::NumMappedBookmarks, NumberOfBookmarks);

	for (int32 BookmarkIndex = 0; BookmarkIndex < NumberOfMappedBookmarks; ++BookmarkIndex)
	{
		if (IBookmarkTypeTools::Get().CheckBookmark(BookmarkIndex, &ViewportClient))
		{
			Section.AddMenuEntry(
				NAME_None,
				FLevelViewportCommands::Get().ClearBookmarkCommands[BookmarkIndex],
				FBookmarkUI::GetPlainLabel(BookmarkIndex)
			);
		}
	}

	for (int32 BookmarkIndex = NumberOfMappedBookmarks; BookmarkIndex < NumberOfBookmarks; ++BookmarkIndex)
	{
		if (IBookmarkTypeTools::Get().CheckBookmark(BookmarkIndex, &ViewportClient))
		{
			FUIAction Action;
			Action.ExecuteAction.BindSP(SharedViewport.ToSharedRef(), &::SLevelViewport::OnClearBookmark, BookmarkIndex);

			Section.AddMenuEntry(
				NAME_None,
				FBookmarkUI::GetPlainLabel(BookmarkIndex),
				FBookmarkUI::GetClearTooltip(BookmarkIndex),
				FSlateIcon(FAppStyle::GetAppStyleSetName(), "GraphEditor.Clean"),
				Action
			);
		}
	}
}

} // namespace UE::LevelEditor::Private

namespace UE::LevelEditor
{

bool ShowViewportRealtimeWarning(FLevelEditorViewportClient& ViewportClient)
{
	return !ViewportClient.IsRealtime() && !ViewportClient.IsRealtimeOverrideSet() && ViewportClient.IsPerspective();
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
					PrimitiveColorHandler.HandlerToolTipText,
					FSlateIcon(),
					FUIAction(
						FExecuteAction::CreateLambda(
							[WeakViewport, PrimitiveColorHandler]()
							{
								if (TSharedPtr<::SLevelViewport> Viewport = WeakViewport.Pin())
								{
									FLevelEditorViewportClient& ViewportClient = Viewport->GetLevelViewportClient();
									ViewportClient.ChangeActorColorationVisualizationMode(PrimitiveColorHandler.HandlerName);
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
									FLevelEditorViewportClient& ViewportClient = Viewport->GetLevelViewportClient();
									return ViewportClient.IsActorColorationVisualizationModeSelected(PrimitiveColorHandler.HandlerName) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
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
			"VisualizeActorColorationViewMode",
			LOCTEXT("VisualizeActorColorationViewModeDisplayName", "Actor Coloration"),
			LOCTEXT("ActorColorationVisualizationMenu_ToolTip", "Select a mode for actor coloration visualization."),
			FNewToolMenuDelegate::CreateLambda(BuildActorColorationMenu),
			FUIAction(
				FExecuteAction(),
				FCanExecuteAction(),
				FIsActionChecked::CreateLambda(
					[WeakViewport = InViewport.ToWeakPtr()]()
					{
						const TSharedPtr<::SLevelViewport> Viewport = WeakViewport.Pin();
						check(Viewport.IsValid());
						FLevelEditorViewportClient& ViewportClient = Viewport->GetLevelViewportClient();
						return ViewportClient.IsViewModeEnabled(VMI_VisualizeActorColoration);
					}
				)
			),
			EUserInterfaceActionType::RadioButton,
			/* bInOpenSubMenuOnClick = */ false,
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "EditorViewport.VisualizeActorColorationMode")
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

void ExtendViewModesSubmenu(FName InViewModesSubmenuName)
{
	UToolMenu* const Submenu = UToolMenus::Get()->ExtendMenu(InViewModesSubmenuName);

	Submenu->AddDynamicSection(
		"LevelEditorViewModesExtensionDynamicSection",
		FNewToolMenuDelegate::CreateLambda(
			[](UToolMenu* InDynamicMenu)
			{
				ULevelViewportContext* const LevelViewportContext = InDynamicMenu->FindContext<ULevelViewportContext>();
				if (!LevelViewportContext)
				{
					return;
				}

				if (const TSharedPtr<::SLevelViewport> LevelViewport = LevelViewportContext->LevelViewport.Pin())
				{
					PopulateViewModesMenu(InDynamicMenu, LevelViewport.ToSharedRef());
				}
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

								// double SLevelViewportToolBar::OnGetHLODInEditorMaxDrawDistanceValue() const
								auto OnGetHLODInEditorMaxDrawDistanceValue = []() -> double
								{
									IWorldPartitionEditorModule* WorldPartitionEditorModule =
										FModuleManager::GetModulePtr<IWorldPartitionEditorModule>("WorldPartitionEditor");
									return WorldPartitionEditorModule
										? WorldPartitionEditorModule->GetHLODInEditorMaxDrawDistance()
										: 0;
								};

								// void SLevelViewportToolBar::OnHLODInEditorMaxDrawDistanceValueChanged(double NewValue) const
								auto OnHLODInEditorMaxDrawDistanceValueChanged = [](double NewValue) -> void
								{
									IWorldPartitionEditorModule* WorldPartitionEditorModule =
										FModuleManager::GetModulePtr<IWorldPartitionEditorModule>("WorldPartitionEditor");
									if (WorldPartitionEditorModule)
									{
										WorldPartitionEditorModule->SetHLODInEditorMaxDrawDistance(NewValue);
										GEditor->RedrawLevelEditingViewports(true);
									}
								};

								TSharedRef<SSpinBox<double>> MaxDrawDistanceSpinBox =
									SNew(SSpinBox<double>)
										.MinValue(MaxDrawDistanceMinValue)
										.MaxValue(MaxDrawDistanceMaxValue)
										.IsEnabled(bHLODInEditorAllowed)
										.Value_Lambda(OnGetHLODInEditorMaxDrawDistanceValue)
										.OnValueChanged_Lambda(OnHLODInEditorMaxDrawDistanceValueChanged)
										.ToolTipText(
											bHLODInEditorAllowed
												? LOCTEXT(
													  "HLODsInEditor_MaxDrawDistance_Tooltip",
													  "Sets the maximum distance at which HLODs will be rendered (0.0 means infinite)"
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
									// clang-format off
									return SNew(SBox)
										.HAlign(HAlign_Right)
										[
											SNew(SBox)
										  .Padding(FMargin(0.0f, 0.0f, 0.0f, 0.0f))
										  .WidthOverride(100.0f)
											[
												SNew(SBorder)
												.BorderImage(FAppStyle::Get().GetBrush("Menu.WidgetBorder"))
												.Padding(FMargin(1.0f))
												[
													InSpinBoxWidget
												]
											]
										];
									// clang-format on
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

void CreateCameraSpawnMenu(UToolMenu* InMenu)
{
	FToolMenuSection& Section = InMenu->AddSection("Section");
	const FLevelViewportCommands& Actions = FLevelViewportCommands::Get();

	for (TSharedPtr<FUICommandInfo> Camera : Actions.CreateCameras)
	{
		Section.AddMenuEntry(NAME_None, Camera);
	}
}

void CreateBookmarksMenu(UToolMenu* InMenu, TWeakPtr<::SLevelViewport> InViewport)
{
	// Add a menu entry for each bookmark
	TSharedPtr<::SLevelViewport> SharedViewport = InViewport.Pin();
	FLevelEditorViewportClient& ViewportClient = SharedViewport->GetLevelViewportClient();

	FToolMenuSection& ManageBookmarksSection =
		InMenu->FindOrAddSection("ManageBookmarks", LOCTEXT("ManageBookmarkSectionName", "Manage Bookmarks"));

	bool bFoundBookmarks = false;

	// Jump to Bookmark Section
	{
		bFoundBookmarks = Private::AddJumpToBookmarkMenu(InMenu, InViewport);
	}

	// Manage Bookmarks Section
	{
		// Set Bookmark Submenu
		{
			const int32 NumberOfBookmarks =
				static_cast<int32>(IBookmarkTypeTools::Get().GetMaxNumberOfBookmarks(&ViewportClient));
			const int32 NumberOfMappedBookmarks = FMath::Min<int32>(AWorldSettings::NumMappedBookmarks, NumberOfBookmarks);

			ManageBookmarksSection.AddSubMenu(
				"SetBookmark",
				LOCTEXT("SetBookmarkSubMenu", "Set Bookmark"),
				LOCTEXT("SetBookmarkSubMenu_ToolTip", "Setting bookmarks"),
				FNewToolMenuDelegate::CreateLambda(
					[NumberOfMappedBookmarks](UToolMenu* InMenu)
					{
						const FLevelViewportCommands& Actions = FLevelViewportCommands::Get();

						FToolMenuSection& SetBookmarksSection =
							InMenu->FindOrAddSection("SetBookmark", LOCTEXT("SetBookmarkSectionName", "Set Bookmark"));

						for (int32 BookmarkIndex = 0; BookmarkIndex < NumberOfMappedBookmarks; ++BookmarkIndex)
						{
							SetBookmarksSection.AddMenuEntry(
								NAME_None,
								Actions.SetBookmarkCommands[BookmarkIndex],
								FBookmarkUI::GetPlainLabel(BookmarkIndex),
								FBookmarkUI::GetSetTooltip(BookmarkIndex),
								FSlateIcon(FAppStyle::Get().GetStyleSetName(), "LevelViewport.ToggleActorPilotCameraView")
							);
						}
					}
				),
				false,
				FSlateIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelViewport.ToggleActorPilotCameraView"))
			);
		}

		// Manage Bookmarks Submenu
		{
			if (bFoundBookmarks)
			{
				ManageBookmarksSection.AddSubMenu(
					"ManageBookmarks",
					LOCTEXT("ManageBookmarksSubMenu", "Manage Bookmarks"),
					LOCTEXT("ManageBookmarksSubMenu_ToolTip", "Bookmarks related actions"),
					FNewToolMenuDelegate::CreateLambda(
						[bFoundBookmarks, InViewport](UToolMenu* InMenu)
						{
							if (!bFoundBookmarks)
							{
								return;
							}

							const FLevelViewportCommands& Actions = FLevelViewportCommands::Get();

							FToolMenuSection& ManageBookmarksSubsection = InMenu->FindOrAddSection(
								"ManageBookmarks", LOCTEXT("ManageBookmarkSectionName", "Manage Bookmarks")
							);

							ManageBookmarksSubsection.AddSubMenu(
								"ClearBookmark",
								LOCTEXT("ClearBookmarkSubMenu", "Clear Bookmark"),
								LOCTEXT("ClearBookmarkSubMenu_ToolTip", "Clear viewport bookmarks"),
								FNewToolMenuDelegate::CreateLambda(&Private::AddClearBookmarkMenu, InViewport),
								false,
								FSlateIcon(FAppStyle::Get().GetStyleSetName(), "EditorViewport.SubMenu.Bookmarks")
							);

							FToolMenuEntry& CompactBookmarks =
								ManageBookmarksSubsection.AddMenuEntry(Actions.CompactBookmarks);
							CompactBookmarks.Icon =
								FSlateIcon(FAppStyle::GetAppStyleSetName(), "AnimationEditor.ApplyCompression");

							FToolMenuEntry& ClearBookmarks =
								ManageBookmarksSubsection.AddMenuEntry(Actions.ClearAllBookmarks);
							ClearBookmarks.Icon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "GraphEditor.Clean");
						}
					),
					false,
					FSlateIcon(FAppStyle::Get().GetStyleSetName(), "EditorViewport.SubMenu.Bookmarks")
				);
			}
		}
	}
}

FToolMenuEntry CreateFOVMenu(TWeakPtr<::SLevelViewport> InLevelViewportWeak)
{
	constexpr float FOVMin = 5.0f;
	constexpr float FOVMax = 170.0f;

	return UnrealEd::CreateNumericEntry(
		"FOVAngle",
		LOCTEXT("FOVAngle", "Field of View"),
		LOCTEXT("FOVAngleTooltip", "Field of View"),
		FCanExecuteAction(),
		UnrealEd::FNumericEntryExecuteActionDelegate::CreateLambda(
			[InLevelViewportWeak](float InValue)
			{
				if (TSharedPtr<::SLevelViewport> LevelViewport = InLevelViewportWeak.Pin())
				{
					Private::SetLevelViewportFOV(LevelViewport.ToSharedRef(), InValue);
				}
			}
		),
		TAttribute<float>::CreateLambda(
			[InLevelViewportWeak]()
			{
				if (TSharedPtr<::SLevelViewport> Viewport = InLevelViewportWeak.Pin())
				{
					return Private::GetLevelViewportFOV(Viewport.ToSharedRef());
				}

				return FOVMin;
			}
		),
		FOVMin,
		FOVMax,
		1
	);
}

FToolMenuEntry CreateFarViewPlaneMenu(TWeakPtr<::SLevelViewport> InInLevelViewportWeak)
{
	constexpr float FarMin = 0.0f;
	constexpr float FarMax = 100000.0f;

	return UnrealEd::CreateNumericEntry(
		"FarViewPlane",
		LOCTEXT("FarViewPlane", "Far View Plane"),
		LOCTEXT("FarViewPlaneTooltip", "Far View Plane"),
		FCanExecuteAction(),
		UnrealEd::FNumericEntryExecuteActionDelegate::CreateLambda(
			[InInLevelViewportWeak](float InValue)
			{
				if (TSharedPtr<::SLevelViewport> LevelViewport = InInLevelViewportWeak.Pin())
				{
					Private::SetFarViewPlaneValue(LevelViewport.ToSharedRef(), InValue);
				}
			}
		),
		TAttribute<float>::CreateLambda(
			[InInLevelViewportWeak]()
			{
				if (TSharedPtr<::SLevelViewport> Viewport = InInLevelViewportWeak.Pin())
				{
					return Private::GetFarViewPlaneValue(Viewport.ToSharedRef());
				}

				return FarMax;
			}
		),
		FarMin,
		FarMax,
		1
	);
}

FToolMenuEntry CreateCameraSpeedSlider(TWeakPtr<::SLevelViewport> InLevelViewportWeak)
{
	constexpr float MinSpeed = 0.033f;
	constexpr float MaxSpeed = 32.0f;

	return UnrealEd::CreateNumericEntry(
		"CameraSpeed",
		LOCTEXT("CameraSpeedLabel", "Camera Speed"),
		LOCTEXT("CameraSpeedTooltip", "Camera Speed"),
		FCanExecuteAction(),
		UnrealEd::FNumericEntryExecuteActionDelegate::CreateLambda(
			[InLevelViewportWeak](float InValue)
			{
				if (TSharedPtr<::SLevelViewport> LevelViewport = InLevelViewportWeak.Pin())
				{
					Private::SetCameraSpeed(LevelViewport.ToSharedRef(), InValue);
				}
			}
		),
		TAttribute<float>::CreateLambda(
			[InLevelViewportWeak]()
			{
				if (TSharedPtr<::SLevelViewport> Viewport = InLevelViewportWeak.Pin())
				{
					return Private::GetCamSpeedSliderPosition(Viewport.ToSharedRef());
				}

				return 1.0f;
			}
		),
		MinSpeed,
		MaxSpeed,
		3
	);
}

FToolMenuEntry CreateCameraSpeedScalarSlider(TWeakPtr<::SLevelViewport> InLevelViewportWeak)
{
	constexpr float MinSpeed = 1.0f;
	constexpr float MaxSpeed = 128.0f;

	return UnrealEd::CreateNumericEntry(
		"CameraSpeedScalar",
		LOCTEXT("CameraSpeedScalarLabel", "Speed Scalar"),
		LOCTEXT("CameraSpeedScalarTooltip", "Scalar to increase camera movement range"),
		FCanExecuteAction(),
		UnrealEd::FNumericEntryExecuteActionDelegate::CreateLambda(
			[InLevelViewportWeak](float InValue)
			{
				if (TSharedPtr<::SLevelViewport> LevelViewport = InLevelViewportWeak.Pin())
				{
					Private::SetCameraSpeedScalarValue(LevelViewport.ToSharedRef(), InValue);
				}
			}
		),
		TAttribute<float>::CreateLambda(
			[InLevelViewportWeak]()
			{
				if (TSharedPtr<::SLevelViewport> Viewport = InLevelViewportWeak.Pin())
				{
					return Private::GetCamSpeedScalarSliderPosition(Viewport.ToSharedRef());
				}

				return MinSpeed;
			}
		),
		MinSpeed,
		MaxSpeed,
		1
	);
}

void CreateCameraSpeedMenu(UToolMenu* InMenu, const TWeakPtr<::SLevelViewport>& InLevelViewportWeak)
{
	FToolMenuSection& Section = InMenu->AddSection("Section");

	Section.AddEntry(CreateCameraSpeedSlider(InLevelViewportWeak));
	Section.AddEntry(CreateCameraSpeedScalarSlider(InLevelViewportWeak));
}

FToolMenuEntry CreateLevelEditorViewportToolbarCameraSubmenu()
{
	return FToolMenuEntry::InitSubMenu(
		"Camera",
		LOCTEXT("CameraSubmenuLabel", "Camera"),
		LOCTEXT("CameraSubmenuTooltip", "Viewport-related Camera settings"),
		FNewToolMenuDelegate::CreateLambda(
			[](UToolMenu* Submenu) -> void
			{
				TWeakPtr<::SLevelViewport> LevelViewportWeak = Submenu->FindContext<ULevelViewportContext>()->LevelViewport;

				// TODO:
				// add Select Active Camera

				// Perspective Section
				{
					FToolMenuSection& PerspectiveSection =
						Submenu->FindOrAddSection("Perspective", LOCTEXT("PerspectiveLabel", "Perspective"));

					PerspectiveSection.AddMenuEntry(FEditorViewportCommands::Get().Perspective);

					// TODO: show separator based on actual list of cameras
					// PerspectiveSection.AddSeparator("PerspectiveSeparator");
					// Camera list
					// Camera types
				}

				// Orthographic Section
				{
					FToolMenuSection& OrthographicSection =
						Submenu->FindOrAddSection("Orthographic", LOCTEXT("OrthographicLabel", "Orthographic"));
					OrthographicSection.AddMenuEntry(FEditorViewportCommands::Get().Top);
					OrthographicSection.AddMenuEntry(FEditorViewportCommands::Get().Bottom);
					OrthographicSection.AddMenuEntry(FEditorViewportCommands::Get().Left);
					OrthographicSection.AddMenuEntry(FEditorViewportCommands::Get().Right);
					OrthographicSection.AddMenuEntry(FEditorViewportCommands::Get().Front);
					OrthographicSection.AddMenuEntry(FEditorViewportCommands::Get().Back);

					OrthographicSection.AddSeparator("PerspectiveSeparator");

					OrthographicSection.AddEntry(CreateFOVMenu(LevelViewportWeak));
					OrthographicSection.AddEntry(CreateFarViewPlaneMenu(LevelViewportWeak));
				}

				// Create Section
				{
					FToolMenuSection& CreateSection = Submenu->FindOrAddSection("Create", LOCTEXT("CreateLabel", "Create"));

					CreateSection.AddSubMenu(
						"CreateCamera",
						LOCTEXT("CameraSubMenu", "Create Camera Here"),
						LOCTEXT("CameraSubMenu_ToolTip", "Select a camera type to create at current viewport's location"),
						FNewToolMenuDelegate::CreateLambda(
							[](UToolMenu* InMenu)
							{
								CreateCameraSpawnMenu(InMenu);
							}
						),
						false,
						FSlateIcon(FAppStyle::Get().GetStyleSetName(), "EditorViewport.SubMenu.CreateCamera")
					);

					CreateSection.AddSubMenu(
						"Bookmarks",
						LOCTEXT("BookmarksSubMenu", "Bookmarks"),
						LOCTEXT("BookmarksSubMenu_ToolTip", "Bookmarks related actions"),
						FNewToolMenuDelegate::CreateLambda(
							[LevelViewportWeak](UToolMenu* InMenu)
							{
								CreateBookmarksMenu(InMenu, LevelViewportWeak);
							}
						),
						false,
						FSlateIcon(FAppStyle::Get().GetStyleSetName(), "EditorViewport.SubMenu.Bookmarks")
					);
				}

				// Positioning Section
				{
					FToolMenuSection& PositioningSection =
						Submenu->FindOrAddSection("Positioning", LOCTEXT("PositioningLabel", "Positioning"));

					// Camera Speed Submenu
					{
						PositioningSection.AddSubMenu(
							"CameraSpeed",
							LOCTEXT("CameraSpeedSubMenu", "Camera Speed"),
							LOCTEXT("CameraSpeedSubMenu_ToolTip", "Camera Speed related actions"),
							FNewToolMenuDelegate::CreateLambda(
								[LevelViewportWeak](UToolMenu* InMenu)
								{
									CreateCameraSpeedMenu(InMenu, LevelViewportWeak);
								}
							),
							false,
							FSlateIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelViewport.ToggleActorPilotCameraView"))
						);
					}

					PositioningSection.AddSeparator("PositioningSeparator_1");

					// Frame Selection
					{
						FToolMenuEntry FocusViewportToSelection =
							FToolMenuEntry::InitMenuEntry(FEditorViewportCommands::Get().FocusViewportToSelection);
						FocusViewportToSelection.UserInterfaceActionType = EUserInterfaceActionType::ToggleButton;
						FocusViewportToSelection.Icon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.FrameActor");
						PositioningSection.AddEntry(FocusViewportToSelection);
					}

					// Pilot Submenu
					{
						PositioningSection.AddSubMenu(
							"Pilot",
							LOCTEXT("PilotSubMenu", "Pilot"),
							LOCTEXT("PilotSubMenu_ToolTip", "Pilot related actions"),
							FNewToolMenuDelegate::CreateLambda(
								[](UToolMenu* InMenu)
								{
									// TODO add create Pilot menu
								}
							),
							false,
							FSlateIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelViewport.PilotSelectedActor"))
						);
					}

					PositioningSection.AddSeparator("PositioningSeparator_2");

					// Move Camera/Object
					{
						FToolMenuEntry CameraToObjectMenu =
							FToolMenuEntry::InitMenuEntry(FLevelEditorCommands::Get().SnapCameraToObject);
						CameraToObjectMenu.UserInterfaceActionType = EUserInterfaceActionType::ToggleButton;
						CameraToObjectMenu.Label = LOCTEXT("CameraToObjectLabel", "Move Camera to Object");
						PositioningSection.AddEntry(CameraToObjectMenu);

						FToolMenuEntry ObjectToCameraMenu =
							FToolMenuEntry::InitMenuEntry(FLevelEditorCommands::Get().SnapObjectToCamera);
						ObjectToCameraMenu.UserInterfaceActionType = EUserInterfaceActionType::ToggleButton;
						ObjectToCameraMenu.Label = LOCTEXT("ObjectToCameraLabel", "Move Object to Camera");
						PositioningSection.AddEntry(ObjectToCameraMenu);
					}
				}

				// Options Section
				{
					FToolMenuSection& OptionsSection =
						Submenu->FindOrAddSection("CameraOptions", LOCTEXT("OptionsLabel", "Options"));
					// add Cinematic Viewport
					// add Allow Cinematic Control
					// add Game View

					FToolMenuEntry AllowCinematicControl =
						FToolMenuEntry::InitMenuEntry(FLevelViewportCommands::Get().ToggleCinematicPreview);
					AllowCinematicControl.UserInterfaceActionType = EUserInterfaceActionType::ToggleButton;
					OptionsSection.AddEntry(AllowCinematicControl);

					FToolMenuEntry ToggleGameView =
						FToolMenuEntry::InitMenuEntry(FLevelViewportCommands::Get().ToggleGameView);
					ToggleGameView.UserInterfaceActionType = EUserInterfaceActionType::ToggleButton;
					OptionsSection.AddEntry(ToggleGameView);

					// This additional options section is used to force certain elements to appear after extensions
					{
						FToolMenuSection& AdditionalOptions =
							Submenu->FindOrAddSection("AdditionalOptions", LOCTEXT("AdditionalOptionsLabel", ""));
						AdditionalOptions.AddSeparator("AdditionalOptionsSeparator");

						FToolMenuEntry HighResolutionScreenshot =
							FToolMenuEntry::InitMenuEntry(FLevelViewportCommands::Get().HighResScreenshot);
						HighResolutionScreenshot.UserInterfaceActionType = EUserInterfaceActionType::ToggleButton;
						AdditionalOptions.AddEntry(HighResolutionScreenshot);
					}
				}
			}
		)
	);
}

} // namespace UE::LevelEditor

#undef LOCTEXT_NAMESPACE
