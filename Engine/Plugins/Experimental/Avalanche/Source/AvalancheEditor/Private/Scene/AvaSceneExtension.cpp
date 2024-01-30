// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaSceneExtension.h"
#include "AvaSceneSettingsTabSpawner.h"
#include "Engine/World.h"
#include "Scene/AvaSceneDefaults.h"
#include "ScopedTransaction.h"
#include "ToolMenu.h"
#include "ToolMenuEntry.h"
#include "ToolMenuSection.h"

#define LOCTEXT_NAMESPACE "AvaSceneExtension"

void FAvaSceneExtension::ExtendToolbarMenu(UToolMenu& InMenu)
{
	FToolMenuSection& Section = InMenu.FindOrAddSection(DefaultSectionName);

	FToolMenuEntry& Entry = Section.AddEntry(FToolMenuEntry::InitToolBarButton(TEXT("SpawnDefaultsButton")
		, FExecuteAction::CreateSP(this, &FAvaSceneExtension::SpawnDefaultScene)
		, LOCTEXT("SpawnDefaultSceneLabel", "Spawn Defaults")
		, LOCTEXT("SpawnDefaultSceneTooltip", "Open the spawn defaults menu to add a basic scene setup.")
		, FSlateIcon(FAppStyle::GetAppStyleSetName(), "GraphEditor.SpawnActor_16x")));

	Entry.StyleNameOverride = "CalloutToolbar";
}

void FAvaSceneExtension::RegisterTabSpawners(const TSharedRef<IAvaEditor>& InEditor) const
{
	InEditor->AddTabSpawner<FAvaSceneSettingsTabSpawner>(InEditor);
}

void FAvaSceneExtension::SpawnDefaultScene()
{
	UWorld* const World = GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	FScopedTransaction Transaction(LOCTEXT("SpawnDefaultScene", "Spawn Default Scene"));

	FAvaSceneDefaults::CreateDefaultScene(World);
}

#undef LOCTEXT_NAMESPACE
