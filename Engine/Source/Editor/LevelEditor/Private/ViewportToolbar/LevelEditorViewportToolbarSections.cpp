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
	FToolMenuSection& Section = InMenu->FindOrAddSection("Transforms", LOCTEXT("TransformsSectionLabel", "Transforms"));

	Section.AddEntry(FToolMenuEntry::InitToolBarButton(FEditorViewportCommands::Get().SelectMode));
	Section.AddEntry(FToolMenuEntry::InitToolBarButton(FEditorViewportCommands::Get().TranslateMode));
	Section.AddEntry(FToolMenuEntry::InitToolBarButton(FEditorViewportCommands::Get().RotateMode));
	Section.AddEntry(FToolMenuEntry::InitToolBarButton(FEditorViewportCommands::Get().ScaleMode));
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
	FToolMenuSection& Section = InMenu->FindOrAddSection("Settings", LOCTEXT("SettingsSectionLabel", "Settings"));
	Section.SetShowSectionMenu(true);

	// Add realtime rendering toggle.
	Section.AddEntry(FToolMenuEntry::InitToolBarButton(FEditorViewportCommands::Get().ToggleRealTime));

	AddMaterialQualityLevelSubmenu(Section);
	AddFeatureLevelPreviewSubmenu(Section);

	// Add maximize/restore viewport button.
	{
		FToolUIAction MaximizeRestoreAction;
		MaximizeRestoreAction.ExecuteAction = FToolMenuExecuteAction::CreateLambda([](const FToolMenuContext& Context) {
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

		Section.AddEntry(FToolMenuEntry::InitToolBarButton("MaximizeRestore", MaximizeRestoreAction,
			LOCTEXT("MaximizeRestoreLabel", "Maximize/restore"),
			LOCTEXT("MaximizeRestoreTooltip", "Maximizes or restores this viewport"),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "EditorViewportToolBar.Maximize.Normal"),
			EUserInterfaceActionType::ToggleButton));
	}

	// Add immersive mode toggle.
	Section.AddEntry(FToolMenuEntry::InitToolBarButton(FLevelViewportCommands::Get().ToggleImmersive));
}

} // namespace UE::LevelEditor

#undef LOCTEXT_NAMESPACE
