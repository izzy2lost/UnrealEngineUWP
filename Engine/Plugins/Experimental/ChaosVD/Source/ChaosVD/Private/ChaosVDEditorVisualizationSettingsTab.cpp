// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosVDEditorVisualizationSettingsTab.h"

#include "ChaosVDEditorSettings.h"
#include "ChaosVDEngine.h"
#include "ChaosVDPlaybackController.h"
#include "ChaosVDPlaybackViewportTab.h"
#include "ChaosVDStyle.h"
#include "DetailsViewArgs.h"
#include "Framework/Docking/TabManager.h"
#include "IDetailsView.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "Templates/SharedPointer.h"
#include "Widgets/SChaosVDVisualizationControls.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "ChaosVisualDebugger"

TSharedRef<SDockTab> FChaosVDEditorVisualizationSettingsTab::HandleTabSpawned(const FSpawnTabArgs& Args)
{

	TSharedPtr<SChaosVDPlaybackViewport> ViewportWidget;
	if (TSharedPtr<FChaosVDPlaybackViewportTab> ViewportTabSharedPtr = OwningTabWidget->GetPlaybackViewportTab().Pin())
	{
		ViewportWidget = ViewportTabSharedPtr->GetPlaybackViewportWidget().Pin();
	}
	
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.bAllowSearch = false;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;

	TSharedRef<IDetailsView> DetailsPanel = PropertyEditorModule.CreateDetailView(DetailsViewArgs);

	DetailsPanel->SetObject(GetMutableDefault<UChaosVDEditorSettings>());

	TSharedRef<SDockTab> DetailsPanelTab =
		SNew(SDockTab)
		.TabRole(ETabRole::MajorTab)
		.Label(LOCTEXT("ChaosVDEditorSettings", "Chaos VD Settings"))
		.ToolTipText(LOCTEXT("ChaosVDEditorSettingsTip", "See the available settings for the editor"));

	if (ensure(ViewportWidget.IsValid()))
	{
		DetailsPanelTab->SetContent
		(
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			[
				SNew(SChaosVDVisualizationControls, OwningTabWidget->GetChaosVDEngineInstance()->GetPlaybackController(), ViewportWidget)
			]	
		);
	}
	else
	{
		DetailsPanelTab->SetContent
		(
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("ChaosVDEditorSettingsLoadError", "Failed to load Visualization Controls"))
			]	
		);
	}

	DetailsPanelTab->SetTabIcon(FChaosVDStyle::Get().GetBrush("TabIconDetailsPanel"));

	return DetailsPanelTab;
}

#undef LOCTEXT_NAMESPACE 
