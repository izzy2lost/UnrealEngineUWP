// Copyright Epic Games, Inc. All Rights Reserved.

#include "ViewportToolbar/LevelEditorViewportToolbarSections.h"

#include "EditorViewportCommands.h"
#include "LevelEditorActions.h"
#include "LevelViewportActions.h"
#include "LevelViewportContext.h"
#include "SCommonEditorViewportToolbarBase.h"
#include "SLevelViewport.h"
#include "SScalabilitySettings.h"
#include "Templates/SharedPointer.h"
#include "ToolMenu.h"
#include "ToolMenus.h"
#include "Widgets/Input/SVolumeControl.h"
#include "Widgets/SBoxPanel.h"

#define LOCTEXT_NAMESPACE "LevelEditorViewportToolbar"

namespace UE::LevelEditor::Private
{

TOptional<bool> UpdateAndGetRealtimeWarningFromContext(const FToolMenuContext& Context)
{
	if (ULevelViewportContext* const LevelViewportContext = Context.FindContext<ULevelViewportContext>())
	{
		if (const TSharedPtr<::SLevelViewport> LevelViewport = LevelViewportContext->LevelViewport.Pin())
		{
			const bool bShowWarning = ShowViewportRealtimeWarning(LevelViewport->GetLevelViewportClient());

			LevelViewportContext->CachedShouldShowRealtimeOffWarning = bShowWarning;

			return LevelViewportContext->CachedShouldShowRealtimeOffWarning;
		}
	}

	return TOptional<bool>();
}

TOptional<bool> IsDirtyRealtimeWarningFromContext(const FToolMenuContext& Context)
{
	if (ULevelViewportContext* const LevelViewportContext = Context.FindContext<ULevelViewportContext>())
	{
		if (const TSharedPtr<::SLevelViewport> LevelViewport = LevelViewportContext->LevelViewport.Pin())
		{
			const bool bShowWarning = ShowViewportRealtimeWarning(LevelViewport->GetLevelViewportClient());
			return bShowWarning != LevelViewportContext->CachedShouldShowRealtimeOffWarning;
		}
	}

	return TOptional<bool>();
}

} // namespace UE::LevelEditor::Private

namespace UE::LevelEditor
{

bool ShowViewportRealtimeWarning(FLevelEditorViewportClient& ViewportClient)
{
	return !ViewportClient.IsRealtime() && !ViewportClient.IsRealtimeOverrideSet() && ViewportClient.IsPerspective();
}

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

void AddFeatureLevelPreviewSubmenu(FToolMenuSection& Section)
{
	Section.AddSubMenu("FeatureLevelPreview",
		NSLOCTEXT("LevelToolBarViewMenu", "PreviewPlatformSubMenu", "Preview Platform"),
		NSLOCTEXT("LevelToolBarViewMenu", "PreviewPlatformSubMenu_ToolTip",
			"Sets the preview platform used by the main editor"),
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

void AddViewportToolbarPerformanceAndScalabilitySubmenu(FToolMenuSection& InSection)
{
	InSection.AddSubMenu("PerformanceAndScalability",
		LOCTEXT("PerformanceAndScalabilityLabel", "Performance & Scalability"),
		LOCTEXT("PerformanceAndScalabilityLabel", "Performance and scalability tools tied to this viewport."),
		FNewToolMenuDelegate::CreateLambda([](UToolMenu* Submenu) -> void {
			{
				FToolMenuSection& UnnamedSection = Submenu->FindOrAddSection(NAME_None);

				// Add realtime rendering toggle.
				UnnamedSection.AddDynamicEntry("ToggleRealtimeDynamicSection",
					FNewToolMenuSectionDelegate::CreateLambda([](FToolMenuSection& InnerSection) -> void {
						FToolUIAction RealtimeToggleAction;
						RealtimeToggleAction.ExecuteAction = FToolMenuExecuteAction::CreateLambda(
							[](const FToolMenuContext& Context) -> void {
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
									UToolMenus::Get()->RefreshAllWidgets();
								}
							});

						RealtimeToggleAction.GetActionCheckState = FToolMenuGetActionCheckState::CreateLambda(
							[](const FToolMenuContext& Context) -> ECheckBoxState {
								ULevelViewportContext* const LevelViewportContext =
									Context.FindContext<ULevelViewportContext>();
								if (!LevelViewportContext)
								{
									return ECheckBoxState::Undetermined;
								}

								// Check if the realtime warn state is outdated and if so refresh widgets to update our
								// top-level status.
								if (const TOptional<bool> IsDirty = Private::IsDirtyRealtimeWarningFromContext(Context);
									IsDirty.IsSet() && IsDirty.GetValue())
								{
									UToolMenus::Get()->RefreshAllWidgets();
								}

								if (const TSharedPtr<::SLevelViewport> LevelViewport =
										LevelViewportContext->LevelViewport.Pin())
								{
									return LevelViewport->IsRealtime() ? ECheckBoxState::Checked
																	   : ECheckBoxState::Unchecked;
								}

								return ECheckBoxState::Undetermined;
							});

						bool bDisplayTopLevel = false;
						if (const TOptional<bool> ShouldWarn = Private::UpdateAndGetRealtimeWarningFromContext(
								InnerSection.Context);
							ShouldWarn.IsSet())
						{
							bDisplayTopLevel = ShouldWarn.GetValue();
						}
						else
						{
							// If we couldn't get the warn state, pretend we don't have to warn.
							bDisplayTopLevel = false;
						}

						const FText Tooltip =
							bDisplayTopLevel
								? LOCTEXT("ToggleRealtimeTooltip_WarnRealtimeOff",
									"This viewport is not updating in realtime.  Click to turn on realtime mode.")
								: LOCTEXT("ToggleRealtimeTooltip", "Toggle realtime rendering of the viewport");

						FToolMenuEntry ToggleRealtime = FToolMenuEntry::InitMenuEntry("ToggleRealtime",
							LOCTEXT("ToggleRealtimeLabel", "Realtime Viewport"), Tooltip,
							FSlateIcon(FAppStyle::GetAppStyleSetName(), "EditorViewport.ToggleRealTime"),
							RealtimeToggleAction, EUserInterfaceActionType::ToggleButton);
						ToggleRealtime.SetShowInToolbarTopLevel(bDisplayTopLevel);
						InnerSection.AddEntry(ToggleRealtime);
					}));
			}

			{
				FToolMenuSection& PerformanceAndScalabilitySection = Submenu->FindOrAddSection(
					"PerformanceAndScalability",
					LOCTEXT("PerformanceAndScalabilitySectionLabel", "Performance & Scalability"));

				AddFeatureLevelPreviewSubmenu(PerformanceAndScalabilitySection);

				PerformanceAndScalabilitySection.AddSeparator("PerformanceAndScalabilitySettings");

				PerformanceAndScalabilitySection.AddSubMenu("Scalability",
					LOCTEXT("ScalabilitySubMenu", "Engine Scalability"),
					LOCTEXT("ScalabilitySubMenu_ToolTip", "Open the engine scalability settings"),
					FNewToolMenuDelegate::CreateLambda([](UToolMenu* InMenu) -> void {
						FToolMenuSection& Section = InMenu->FindOrAddSection(NAME_None);
						Section.AddEntry(
							FToolMenuEntry::InitWidget("ScalabilitySettings", SNew(SScalabilitySettings), FText(), true));
					}));

				AddMaterialQualityLevelSubmenu(PerformanceAndScalabilitySection);

				// FEditorViewportClient& ViewportClient = Viewport.Pin()->GetLevelViewportClient();
				PerformanceAndScalabilitySection.AddSubMenu("ScreenPercentageSubMenu",
					LOCTEXT("ScreenPercentageSubMenu", "Screen Percentage"),
					LOCTEXT("ScreenPercentageSubMenu_ToolTip", "Customize the viewport's screen percentage"),
					FNewToolMenuDelegate::CreateLambda([](UToolMenu* ScreenPercentageSubMenu) {
						FToolMenuSection& UnnamedSection = ScreenPercentageSubMenu->FindOrAddSection(NAME_None);

						ScreenPercentageSubMenu->AddDynamicSection(NAME_None,
							FNewToolMenuDelegateLegacy::CreateLambda([](FMenuBuilder& MenuBuilder, UToolMenu* InMenu) -> void {
								ULevelViewportContext* const LevelViewportContext =
									InMenu->FindContext<ULevelViewportContext>();
								if (!LevelViewportContext)
								{
									return;
								}

								if (const TSharedPtr<::SLevelViewport> LevelViewport =
										LevelViewportContext->LevelViewport.Pin())
								{
									TSharedPtr<FEditorViewportClient> Client = LevelViewport->GetViewportClient();
									SCommonEditorViewportToolbarBase::ConstructScreenPercentageMenu(
										MenuBuilder, Client.Get());
								}
							}));
					}));
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

TSharedRef<SWidget> BuildVolumeControlCustomWidget()
{
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
}

void AddLevelEditorViewportToolbarSettingsSubmenu(FToolMenuSection& InSection)
{
	InSection.AddSubMenu("Settings", LOCTEXT("SettingsSubmenuLabel", "Settings"),
		LOCTEXT("SettingsSubmenuTooltip", "Viewport-related settings"),
		FNewToolMenuDelegate::CreateLambda([](UToolMenu* Submenu) -> void {
			{
				FToolMenuSection& ViewportControlsSection = Submenu->FindOrAddSection(
					"ViewportControls", LOCTEXT("ViewportControlsSectionLabel", "Viewport Controls"));

				ViewportControlsSection.AddSubMenu("ViewportLayouts", LOCTEXT("ViewportLayoutsLabel", "Layouts"),
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
			}

			{
				FToolMenuSection& SettingsSection = Submenu->FindOrAddSection(
					"Settings", LOCTEXT("SettingsSectionLabel", "Settings"));

				SettingsSection.AddEntry(FToolMenuEntry::InitWidget(
					"Volume", BuildVolumeControlCustomWidget(), LOCTEXT("VolumeControlLabel", "Volume")));

				SettingsSection.AddSeparator("ViewportSizeSeparator");

				SettingsSection.AddMenuEntry(FLevelViewportCommands::Get().ToggleImmersive);

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
							ULevelViewportContext* const LevelViewportContext =
								Context.FindContext<ULevelViewportContext>();
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

					SettingsSection
						.AddMenuEntry("MaximizeRestore", LOCTEXT("MaximizeRestoreLabel", "Maximize Viewport"),
							LOCTEXT("MaximizeRestoreTooltip", "Maximizes or restores this viewport"),
							FSlateIcon(FAppStyle::GetAppStyleSetName(), "EditorViewportToolBar.Maximize.Normal"),
							MaximizeRestoreAction, EUserInterfaceActionType::ToggleButton)
						.SetShowInToolbarTopLevel(true);
				}

				SettingsSection.AddSeparator("AdvancedSeparator");

				{
					const FLevelViewportCommands& LevelViewportActions = FLevelViewportCommands::Get();
					SettingsSection.AddMenuEntry(LevelViewportActions.AdvancedSettings);
				}
			}
		}));
}

} // namespace UE::LevelEditor

#undef LOCTEXT_NAMESPACE
