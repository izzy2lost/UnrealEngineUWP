// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Common/TypedElementHandles.h"
#include "Modules/ModuleInterface.h"
#include "Templates/SharedPointer.h"
#include "Delegates/IDelegateInstance.h"
#include "Templates/UniquePtr.h"

class SDockTab;
class SWidget;
class FSpawnTabArgs;
class ISceneOutliner;

namespace UE::Teds::Debug::QueryEditor
{
	class FTedsQueryEditorModel;
}

/**
 * Implements the Scene Outliner module.
 */
class FTedsDebuggerModule
	: public IModuleInterface
{
public:

	FTedsDebuggerModule();

	// IModuleInterface interface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	// Open the TEDS Debugger if not already open, and navigate to the given row. Optionally disabling all filters if the given row doesn't pass them
	void NavigateToRow(TypedElementDataStorage::RowHandle InRow);

private:
	void RegisterTabSpawners();
	void UnregisterTabSpawners();
	TSharedRef<SDockTab> OpenTedsDebuggerTab(const FSpawnTabArgs& SpawnTabArgs);
	TSharedRef<SWidget> CreateTedsDebugger();

	TSharedRef<SDockTab> OpenQueryEditorTab(const FSpawnTabArgs& SpawnTabArgs);

private:
	FDelegateHandle LevelEditorTabManagerChangedHandle;
	FName TedsDebuggerTabName;
	TypedElementDataStorage::QueryHandle InitialColumnQuery;
	TWeakPtr<ISceneOutliner> TedsDebuggerInstance;

	TUniquePtr<UE::Teds::Debug::QueryEditor::FTedsQueryEditorModel> QueryEditorModel;

};