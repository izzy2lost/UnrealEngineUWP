// Copyright Epic Games, Inc. All Rights Reserved.

#include "ViewportToolbar/LevelEditorViewportToolbarSections.h"

#include "EditorViewportCommands.h"
#include "LevelEditorActions.h"
#include "LevelViewportActions.h"
#include "LevelViewportContext.h"
#include "SLevelViewport.h"
#include "ToolMenu.h"

#define LOCTEXT_NAMESPACE "LevelEditorViewportToolbar"

namespace UE::LevelEditor
{

// TODO: Move this outside the level editor and make it publicly available to anyone building a viewport toolbar.
void AddViewportToolbarTransformsSection(UToolMenu* InMenu)
{
	FToolMenuSection& Section = InMenu->FindOrAddSection("Left");

	Section.AddSubMenu("Transforms", LOCTEXT("TransformsSubmenuLabel", "Transforms"),
		LOCTEXT("TransformsSubmenuTooltip", "Viewport-related transforms tools"),
		FNewToolMenuDelegate::CreateLambda([](UToolMenu* Submenu) -> void {
			FToolMenuSection& Section = Submenu->FindOrAddSection(NAME_None);

			FToolMenuEntry SelectMode = FToolMenuEntry::InitMenuEntry(FEditorViewportCommands::Get().SelectMode);
			SelectMode.UserInterfaceActionType = EUserInterfaceActionType::RadioButton;
			SelectMode.SetShowInToolbarTopLevel(true);
			Section.AddEntry(SelectMode);

			FToolMenuEntry TranslateMode = FToolMenuEntry::InitMenuEntry(FEditorViewportCommands::Get().TranslateMode);
			TranslateMode.UserInterfaceActionType = EUserInterfaceActionType::RadioButton;
			TranslateMode.SetShowInToolbarTopLevel(true);
			Section.AddEntry(TranslateMode);

			FToolMenuEntry RotateMode = FToolMenuEntry::InitMenuEntry(FEditorViewportCommands::Get().RotateMode);
			RotateMode.UserInterfaceActionType = EUserInterfaceActionType::RadioButton;
			RotateMode.SetShowInToolbarTopLevel(true);
			Section.AddEntry(RotateMode);

			FToolMenuEntry ScaleMode = FToolMenuEntry::InitMenuEntry(FEditorViewportCommands::Get().ScaleMode);
			ScaleMode.UserInterfaceActionType = EUserInterfaceActionType::RadioButton;
			ScaleMode.SetShowInToolbarTopLevel(true);
			Section.AddEntry(ScaleMode);
		}));
}

void AddMaterialQualityLevelSubmenu(FToolMenuSection& Section)
{
	Section.AddSubMenu("MaterialQualityLevel",
		NSLOCTEXT("LevelToolBarViewMenu", "MaterialQualityLevelSubMenu", "Material Quality Level"),
		NSLOCTEXT("LevelToolBarViewMenu",
			"MaterialQualityLevelSubMenu_ToolTip",
			"Sets the value of the CVar \"r.MaterialQualityLevel\" (low=0, high=1, medium=2, Epic=3). This affects "
			"materials via the QualitySwitch material expression."),
		FNewToolMenuDelegate::CreateLambda([](UToolMenu* InMenu) -> void {
			FToolMenuSection& Section = InMenu->AddSection("LevelEditorMaterialQualityLevel",
				NSLOCTEXT("LevelToolBarViewMenu", "MaterialQualityLevelHeading", "Material Quality Level"));
			Section.AddMenuEntry(FLevelEditorCommands::Get().MaterialQualityLevel_Low);
			Section.AddMenuEntry(FLevelEditorCommands::Get().MaterialQualityLevel_Medium);
			Section.AddMenuEntry(FLevelEditorCommands::Get().MaterialQualityLevel_High);
			Section.AddMenuEntry(FLevelEditorCommands::Get().MaterialQualityLevel_Epic);
		}));
}

void AddFeatureLevelPreviewSubmenu(FToolMenuSection& Section)
{
	Section.AddSubMenu("FeatureLevelPreview",
		NSLOCTEXT("LevelToolBarViewMenu", "PreviewPlatformSubMenu", "Preview Platform"),
		NSLOCTEXT(
			"LevelToolBarViewMenu", "PreviewPlatformSubMenu_ToolTip", "Sets the preview platform used by the main editor"),
		FNewToolMenuDelegate::CreateLambda([](UToolMenu* InMenu) -> void {
			FToolMenuSection& Section = InMenu->AddSection(
				"EditorPreviewMode", LOCTEXT("EditorPreviewModeDevices", "Preview Devices"));
			// Preview platforms discovered from ITargetPlatforms.
			for (auto& Item : FLevelEditorCommands::Get().PreviewPlatformOverrides)
			{
				Section.AddMenuEntry(Item);
			}
		}));
}

void AddLevelEditorViewportToolbarSettingsSection(UToolMenu* InMenu)
{
	FToolMenuSection& RightSection = InMenu->FindOrAddSection("Right");

	RightSection.AddSubMenu("Settings", LOCTEXT("SettingsSubmenuLabel", "Settings"),
		LOCTEXT("SettingsSubmenuTooltip", "Viewport-related settings"),
		FNewToolMenuDelegate::CreateLambda([](UToolMenu* Submenu) -> void {
			FToolMenuSection& UnnamedSection = Submenu->FindOrAddSection(NAME_None);

			// Add realtime rendering toggle.
			UnnamedSection.AddMenuEntry(FEditorViewportCommands::Get().ToggleRealTime).SetShowInToolbarTopLevel(true);

			AddMaterialQualityLevelSubmenu(UnnamedSection);
			AddFeatureLevelPreviewSubmenu(UnnamedSection);

			// Add maximize/restore viewport button.
			{
				FToolUIAction MaximizeRestoreAction;
				MaximizeRestoreAction.ExecuteAction = FToolMenuExecuteAction::CreateLambda(
					[](const FToolMenuContext& Context) {
						ULevelViewportContext* const LevelViewportContext = Context.FindContext<ULevelViewportContext>();
						if (!LevelViewportContext)
						{
							return;
						}

						if (const TSharedPtr<::SLevelViewport> LevelViewport = LevelViewportContext->LevelViewport.Pin())
						{
							LevelViewport->OnToggleMaximize();
						}
					});
				MaximizeRestoreAction.GetActionCheckState = FToolMenuGetActionCheckState::CreateLambda(
					[](const FToolMenuContext& Context) -> ECheckBoxState {
						ULevelViewportContext* const LevelViewportContext = Context.FindContext<ULevelViewportContext>();
						if (!LevelViewportContext)
						{
							return ECheckBoxState::Undetermined;
						}

						if (const TSharedPtr<::SLevelViewport> LevelViewport = LevelViewportContext->LevelViewport.Pin())
						{
							return LevelViewport->IsMaximized() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
						}

						return ECheckBoxState::Undetermined;
					});

				UnnamedSection.AddEntry(FToolMenuEntry::InitToolBarButton("MaximizeRestore", MaximizeRestoreAction,
					LOCTEXT("MaximizeRestoreLabel", "Maximize/restore"),
					LOCTEXT("MaximizeRestoreTooltip", "Maximizes or restores this viewport"),
					FSlateIcon(FAppStyle::GetAppStyleSetName(), "EditorViewportToolBar.Maximize.Normal"),
					EUserInterfaceActionType::ToggleButton));
			}

			// Add immersive mode toggle.
			UnnamedSection.AddEntry(FToolMenuEntry::InitToolBarButton(FLevelViewportCommands::Get().ToggleImmersive));
		}));
	}

} // namespace UE::LevelEditor

#undef LOCTEXT_NAMESPACE
