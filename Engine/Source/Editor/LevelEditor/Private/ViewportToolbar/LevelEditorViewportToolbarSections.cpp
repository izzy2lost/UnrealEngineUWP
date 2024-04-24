// Copyright Epic Games, Inc. All Rights Reserved.

#include "ViewportToolbar/LevelEditorViewportToolbarSections.h"

#include "EditorViewportCommands.h"
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

void AddLevelEditorViewportToolbarSettingsSection(UToolMenu* InMenu)
{
	FToolMenuSection& Section = InMenu->FindOrAddSection("Settings", LOCTEXT("SettingsSectionLabel", "Settings"));

	// Add realtime rendering toggle.
	Section.AddEntry(FToolMenuEntry::InitToolBarButton(FEditorViewportCommands::Get().ToggleRealTime));

	// Add maximize/restore viewport button.
	{
		FToolUIAction MaximizeRestoreAction;
		MaximizeRestoreAction.ExecuteAction = FToolMenuExecuteAction::CreateLambda([](const FToolMenuContext& Context) {
			ULevelViewportContext* const LevelViewportContext = Context.FindContext<ULevelViewportContext>();
			if (!LevelViewportContext)
			{
				return;
			}

			if (const TSharedPtr<SLevelViewport> LevelViewport = LevelViewportContext->LevelViewport.Pin())
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

				if (const TSharedPtr<SLevelViewport> LevelViewport = LevelViewportContext->LevelViewport.Pin())
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
