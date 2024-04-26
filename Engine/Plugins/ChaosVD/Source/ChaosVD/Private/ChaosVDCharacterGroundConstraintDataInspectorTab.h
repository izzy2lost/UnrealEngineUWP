// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ChaosVDSceneSelectionObserver.h"
#include "ChaosVDTabSpawnerBase.h"
#include "Templates/SharedPointer.h"

class SChaosVDCharacterGroundConstraintDataInspector;
class SChaosVDSceneQueryDataInspector;

/** Spawns and handles and instance for the visual debugger character ground constraint inspector tab */
class FChaosVDCharacterGroundConstraintDataInspectorTab final : public FChaosVDTabSpawnerBase, public FChaosVDSceneSelectionObserver
{
public:
	FChaosVDCharacterGroundConstraintDataInspectorTab(const FName& InTabID, const TSharedPtr<FTabManager>& InTabManager, const TWeakPtr<SChaosVDMainTab>& InOwningTabWidget)
		: FChaosVDTabSpawnerBase(InTabID, InTabManager, InOwningTabWidget)
	{
	}

	virtual ~FChaosVDCharacterGroundConstraintDataInspectorTab() override;
	virtual TSharedRef<SDockTab> HandleTabSpawnRequest(const FSpawnTabArgs& Args) override;
	virtual void HandleTabClosed(TSharedRef<SDockTab> InTabClosed) override;

	virtual void HandlePostSelectionChange(const UTypedElementSelectionSet* ChangedSelectionSet) override;

	TWeakPtr<SChaosVDCharacterGroundConstraintDataInspector> GetConstraintDataInspectorInstance() const { return ConstraintDataInspector; }

protected:
	TSharedPtr<SChaosVDCharacterGroundConstraintDataInspector> ConstraintDataInspector;
};
