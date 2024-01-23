// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosVDSceneQueryDataInspectorTab.h"

#include "EditorModeManager.h"
#include "Widgets/SChaosVDMainTab.h"
#include "Widgets/SChaosVDSceneQueryDataInspector.h"

#define LOCTEXT_NAMESPACE "ChaosVisualDebugger"

FChaosVDSceneQueryDataInspectorTab::~FChaosVDSceneQueryDataInspectorTab()
{
}

TSharedRef<SDockTab> FChaosVDSceneQueryDataInspectorTab::HandleTabSpawned(const FSpawnTabArgs& Args)
{
	TSharedRef<SDockTab> DetailsPanelTab =
	SNew(SDockTab)
	.TabRole(ETabRole::MajorTab)
	.Label(LOCTEXT("SceneQueryInspectorTab", "Scene Query Data Inspector"))
	.ToolTipText(LOCTEXT("SceneQueryInspectorTabTip", "See the details of the any scene query selected in the viewport"));

	if (const TSharedPtr<SChaosVDMainTab> MainTabPtr = OwningTabWidget.Pin())
	{
		DetailsPanelTab->SetContent
		(
			SAssignNew(SceneQueryDataInspector, SChaosVDSceneQueryDataInspector, GetChaosVDScene(), MainTabPtr->GetEditorModeManager().AsWeak())
		);
	}
	else
	{
		DetailsPanelTab->SetContent(GenerateErrorWidget());
	}

	OnTabSpawned().Broadcast(DetailsPanelTab);

	return DetailsPanelTab;
}

#undef LOCTEXT_NAMESPACE
