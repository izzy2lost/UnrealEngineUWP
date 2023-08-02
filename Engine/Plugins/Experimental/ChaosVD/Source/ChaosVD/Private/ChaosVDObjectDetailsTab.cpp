// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosVDObjectDetailsTab.h"

#include "ChaosVDScene.h"
#include "ChaosVDStyle.h"
#include "Editor.h"
#include "Elements/Framework/TypedElementSelectionSet.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "Templates/SharedPointer.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/SChaosVDDetailsView.h"

#define LOCTEXT_NAMESPACE "ChaosVisualDebugger"

class SSubobjectEditor;


TSharedRef<SDockTab> FChaosVDObjectDetailsTab::HandleTabSpawned(const FSpawnTabArgs& Args)
{
	TSharedPtr<FChaosVDScene> ScenePtr = GetChaosVDScene().Pin();
	check(ScenePtr);

	RegisterSelectionSetObject(ScenePtr->GetElementSelectionSet());

	TSharedRef<SDockTab> DetailsPanelTab =
		SNew(SDockTab)
		.TabRole(ETabRole::MajorTab)
		.Label(LOCTEXT("DetailsPanel", "Details"))
		.ToolTipText(LOCTEXT("DetailsPanelToolTip", "See the details of the selected object"));

	DetailsPanelTab->SetContent
	(
		SAssignNew(DetailsPanelView, SChaosVDDetailsView)
	);

	DetailsPanelTab->SetTabIcon(FChaosVDStyle::Get().GetBrush("TabIconDetailsPanel"));

	return DetailsPanelTab;
}

void FChaosVDObjectDetailsTab::HandlePostSelectionChange(const UTypedElementSelectionSet* ChangedSelectionSet)
{
	TArray<AActor*> SelectedActors = ChangedSelectionSet->GetSelectedObjects<AActor>();

	if (SelectedActors.Num() > 0)
	{
		// We don't support multi selection yet
		ensure(SelectedActors.Num() == 1);

		DetailsPanelView->SetSelectedObject(SelectedActors[0]);
	}
}

#undef LOCTEXT_NAMESPACE
