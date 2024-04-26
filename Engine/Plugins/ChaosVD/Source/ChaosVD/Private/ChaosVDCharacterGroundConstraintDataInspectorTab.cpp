// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosVDCharacterGroundConstraintDataInspectorTab.h"

#include "ChaosVDCharacterGroundConstraintDataProviderInterface.h"
#include "ChaosVDScene.h"
#include "Elements/Framework/TypedElementSelectionSet.h"
#include "Widgets/SChaosVDCharacterGroundConstraintDataInspector.h"

#define LOCTEXT_NAMESPACE "ChaosVisualDebugger"

FChaosVDCharacterGroundConstraintDataInspectorTab::~FChaosVDCharacterGroundConstraintDataInspectorTab()
{
}

TSharedRef<SDockTab> FChaosVDCharacterGroundConstraintDataInspectorTab::HandleTabSpawnRequest(const FSpawnTabArgs& Args)
{
	TSharedRef<SDockTab> DetailsPanelTab =
	SNew(SDockTab)
	.TabRole(ETabRole::MajorTab)
	.Label(LOCTEXT("CharacterGroundConstraintDataInspectorTab", "Character Ground Constraint Inspector"))
	.ToolTipText(LOCTEXT("CharacterGroundConstraintDataInspectorTabToolTip", "See the details of the any character ground constraint selected in the viewport"));

	if (const TSharedPtr<SChaosVDMainTab> MainTabPtr = OwningTabWidget.Pin())
	{
		DetailsPanelTab->SetContent
		(
			SAssignNew(ConstraintDataInspector, SChaosVDCharacterGroundConstraintDataInspector, GetChaosVDScene())
		);
	}
	else
	{
		DetailsPanelTab->SetContent(GenerateErrorWidget());
	}

	HandleTabSpawned(DetailsPanelTab);

	if (const TSharedPtr<FChaosVDScene> ScenePtr = GetChaosVDScene().Pin())
	{
		RegisterSelectionSetObject(ScenePtr->GetElementSelectionSet());
	}

	return DetailsPanelTab;
}

void FChaosVDCharacterGroundConstraintDataInspectorTab::HandleTabClosed(TSharedRef<SDockTab> InTabClosed)
{
	FChaosVDTabSpawnerBase::HandleTabClosed(InTabClosed);

	ConstraintDataInspector.Reset();
}

void FChaosVDCharacterGroundConstraintDataInspectorTab::HandlePostSelectionChange(const UTypedElementSelectionSet* ChangedSelectionSet)
{
	TArray<AActor*> SelectedActors = ChangedSelectionSet->GetSelectedObjects<AActor>();

	if (SelectedActors.Num() > 0)
	{
		// We don't support multi selection yet
		for (AActor* SelectedActor : SelectedActors)
		{
			if (IChaosVDCharacterGroundConstraintDataProviderInterface* ConstraintDataProvider = Cast<IChaosVDCharacterGroundConstraintDataProviderInterface>(SelectedActor))
			{
				if (ConstraintDataProvider->HasCharacterGroundConstraintData())
				{
					ConstraintDataInspector->SetConstraintDataProviderObjectToInspect(ConstraintDataProvider);
					break;
				}
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
