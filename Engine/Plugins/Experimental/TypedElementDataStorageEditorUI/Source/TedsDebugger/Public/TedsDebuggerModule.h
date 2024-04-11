// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Common/TypedElementHandles.h"
#include "Modules/ModuleInterface.h"
#include "Templates/SharedPointer.h"
#include "Delegates/IDelegateInstance.h"

class SDockTab;
class SWidget;
class FSpawnTabArgs;

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

private:
	void RegisterTabSpawners();
	void UnregisterTabSpawners();
	TSharedRef<SDockTab> OpenTedsDebuggerTab(const FSpawnTabArgs& SpawnTabArgs);
	TSharedRef<SWidget> CreateTedsDebugger();

private:
	FDelegateHandle LevelEditorTabManagerChangedHandle;
	FName TedsDebuggerTabName;
	TypedElementDataStorage::QueryHandle InitialColumnQuery;

};