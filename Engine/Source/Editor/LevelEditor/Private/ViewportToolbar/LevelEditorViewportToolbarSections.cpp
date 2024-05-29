// Copyright Epic Games, Inc. All Rights Reserved.

#include "ViewportToolbar/LevelEditorViewportToolbarSections.h"

#include "EditorViewportCommands.h"
#include "LevelEditorActions.h"
#include "LevelViewportActions.h"
#include "LevelViewportContext.h"
#include "SLevelViewport.h"
#include "Templates/SharedPointer.h"
#include "ToolMenu.h"

#define LOCTEXT_NAMESPACE "LevelEditorViewportToolbar"

namespace UE::LevelEditor
{

// TODO: Move this outside the level editor and make it publicly available to anyone building a viewport toolbar.
void AddViewportToolbarTransformsSection(FToolMenuSection& InSection)
{
	InSection.AddSubMenu("Transforms", LOCTEXT("TransformsSubmenuLabel", "Transforms"),
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

void GenerateViewportLayoutsMenu(UToolMenu* InMenu, TSharedPtr<::SLevelViewport> InViewport)
{
	TSharedPtr<FUICommandList> CommandList = InViewport->GetCommandList();

	// Disable searching in this menu because it only contains visual representations of
	// viewport layouts without any searchable text.
	InMenu->bSearchable = false;

	{
		FToolMenuSection& Section = InMenu->AddSection(
			"LevelViewportOnePaneConfigs", LOCTEXT("OnePaneConfigHeader", "One Pane"));

		FSlimHorizontalToolBarBuilder OnePaneButton(CommandList, FMultiBoxCustomization::None);
		OnePaneButton.SetLabelVisibility(EVisibility::Collapsed);
		OnePaneButton.SetStyle(&FAppStyle::Get(), "ViewportLayoutToolbar");

		OnePaneButton.AddToolBarButton(FLevelViewportCommands::Get().ViewportConfig_OnePane);

		Section.AddEntry(FToolMenuEntry::InitWidget("LevelViewportOnePaneConfigs",
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
			FText::GetEmpty(), true));
	}

	{
		FToolMenuSection& Section = InMenu->AddSection(
			"LevelViewportTwoPaneConfigs", LOCTEXT("TwoPaneConfigHeader", "Two Panes"));
		FSlimHorizontalToolBarBuilder TwoPaneButtons(CommandList, FMultiBoxCustomization::None);
		TwoPaneButtons.SetLabelVisibility(EVisibility::Collapsed);
		TwoPaneButtons.SetStyle(&FAppStyle::Get(), "ViewportLayoutToolbar");

		TwoPaneButtons.AddToolBarButton(FLevelViewportCommands::Get().ViewportConfig_TwoPanesH, NAME_None, FText());
		TwoPaneButtons.AddToolBarButton(FLevelViewportCommands::Get().ViewportConfig_TwoPanesV, NAME_None, FText());

		Section.AddEntry(FToolMenuEntry::InitWidget("LevelViewportTwoPaneConfigs",
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
			FText::GetEmpty(), true));
	}

	{
		FToolMenuSection& Section = InMenu->AddSection(
			"LevelViewportThreePaneConfigs", LOCTEXT("ThreePaneConfigHeader", "Three Panes"));
		FSlimHorizontalToolBarBuilder ThreePaneButtons(CommandList, FMultiBoxCustomization::None);
		ThreePaneButtons.SetLabelVisibility(EVisibility::Collapsed);
		ThreePaneButtons.SetStyle(&FAppStyle::Get(), "ViewportLayoutToolbar");

		ThreePaneButtons.AddToolBarButton(FLevelViewportCommands::Get().ViewportConfig_ThreePanesLeft, NAME_None, FText());
		ThreePaneButtons.AddToolBarButton(FLevelViewportCommands::Get().ViewportConfig_ThreePanesRight, NAME_None, FText());
		ThreePaneButtons.AddToolBarButton(FLevelViewportCommands::Get().ViewportConfig_ThreePanesTop, NAME_None, FText());
		ThreePaneButtons.AddToolBarButton(
			FLevelViewportCommands::Get().ViewportConfig_ThreePanesBottom, NAME_None, FText());

		Section.AddEntry(FToolMenuEntry::InitWidget("LevelViewportThreePaneConfigs",
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
			FText::GetEmpty(), true));
	}

	{
		FToolMenuSection& Section = InMenu->AddSection(
			"LevelViewportFourPaneConfigs", LOCTEXT("FourPaneConfigHeader", "Four Panes"));
		FSlimHorizontalToolBarBuilder FourPaneButtons(CommandList, FMultiBoxCustomization::None);
		FourPaneButtons.SetLabelVisibility(EVisibility::Collapsed);
		FourPaneButtons.SetStyle(&FAppStyle::Get(), "ViewportLayoutToolbar");

		FourPaneButtons.AddToolBarButton(FLevelViewportCommands::Get().ViewportConfig_FourPanes2x2, NAME_None, FText());
		FourPaneButtons.AddToolBarButton(FLevelViewportCommands::Get().ViewportConfig_FourPanesLeft, NAME_None, FText());
		FourPaneButtons.AddToolBarButton(FLevelViewportCommands::Get().ViewportConfig_FourPanesRight, NAME_None, FText());
		FourPaneButtons.AddToolBarButton(FLevelViewportCommands::Get().ViewportConfig_FourPanesTop, NAME_None, FText());
		FourPaneButtons.AddToolBarButton(FLevelViewportCommands::Get().ViewportConfig_FourPanesBottom, NAME_None, FText());

		Section.AddEntry(FToolMenuEntry::InitWidget("LevelViewportFourPaneConfigs",
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
			FText::GetEmpty(), true));
	}
}

void AddLevelEditorViewportToolbarSettingsSection(FToolMenuSection& InSection)
{
	InSection.AddSubMenu("Settings", LOCTEXT("SettingsSubmenuLabel", "Settings"),
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

			UnnamedSection.AddSubMenu("ViewportLayouts", LOCTEXT("ViewportLayoutsLabel", "Layouts"),
				LOCTEXT("ViewportLayoutsTooltip", "Configure the layouts of the viewport windows"),
				FNewToolMenuDelegate::CreateLambda([](UToolMenu* InMenu) {
					ULevelViewportContext* const LevelViewportContext = InMenu->FindContext<ULevelViewportContext>();
					if (!LevelViewportContext)
					{
						return;
					}

					if (const TSharedPtr<::SLevelViewport> LevelViewport = LevelViewportContext->LevelViewport.Pin())
					{
						GenerateViewportLayoutsMenu(InMenu, LevelViewport);
					}
				}),
				false, FSlateIcon(FAppStyle::Get().GetStyleSetName(), "Icons.Layout"));
		}));
}

} // namespace UE::LevelEditor

#undef LOCTEXT_NAMESPACE
