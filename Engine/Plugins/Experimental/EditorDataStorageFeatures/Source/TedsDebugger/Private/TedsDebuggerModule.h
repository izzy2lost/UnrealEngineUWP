// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Common/TypedElementHandles.h"
#include "Modules/ModuleInterface.h"
#include "Templates/SharedPointer.h"

class SDockTab;
class SWidget;
class FSpawnTabArgs;
class ISceneOutliner;
class STedsDebugger;

namespace UE::EditorDataStorage::Debug
{
/**
 * Implements the Teds Debugger module.
 */
class FTedsDebuggerModule
	: public IModuleInterface
{
public:

	FTedsDebuggerModule() = default;

	// IModuleInterface interface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	// Open the TEDS Debugger if not already open, and navigate to the given row in the table viewer tab
	void NavigateToRow(TypedElementDataStorage::RowHandle InRow) const;

private:
	void RegisterTabSpawners();
	void UnregisterTabSpawners() const;
	
	TSharedRef<SDockTab> OpenTedsDebuggerTab(const FSpawnTabArgs& SpawnTabArgs);

private:
	TWeakPtr<STedsDebugger> TedsDebuggerInstance;
};
}