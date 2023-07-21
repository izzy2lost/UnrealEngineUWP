// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "ChaosVDTabSpawnerBase.h"
#include "Widgets/SChaosVDMainTab.h"

class SChaosVDNameListPicker;

class FChaosVDEditorVisualizationSettingsTab : public FChaosVDTabSpawnerBase
{
public:
	FChaosVDEditorVisualizationSettingsTab(const FName& InTabID, const TSharedPtr<FTabManager>& InTabManager, SChaosVDMainTab* InOwningTabWidget)
		: FChaosVDTabSpawnerBase(InTabID, InTabManager, InOwningTabWidget)
	{
	}

	virtual TSharedRef<SDockTab> HandleTabSpawned(const FSpawnTabArgs& Args) override;
};
