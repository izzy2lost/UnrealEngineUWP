// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class SWidget;
struct FTopLevelAssetPath;

namespace UE::AnimNext::Editor
{

struct FParameterPickerArgs;

class IAnimNextEditorModule : public IModuleInterface
{
public:
	// Create a parameter picker
	// @param  InArgs          Arguments used for configuring the picker
	virtual TSharedRef<SWidget> CreateParameterPicker(const FParameterPickerArgs& InArgs) = 0;

	// Register a valid fragment type name to be used with parameter UOLs
	// @param InLocatorFragmentEditorName The name of the locator fragment editor
	virtual void RegisterLocatorFragmentEditorType(FName InLocatorFragmentEditorName) = 0;

	// Unregister a valid fragment type name to be used with parameter UOLs
	// @param InLocatorFragmentEditorName The name of the locator fragment editor
	virtual void UnregisterLocatorFragmentEditorType(FName InLocatorFragmentEditorName) = 0;

	// Add a UClass path to the set of classes which can be opened within an AnimNext Workspace
	// @param InClassAssetPath Asset path for to-be-registered Class 
	virtual void AddWorkspaceSupportedAssetClass(const FTopLevelAssetPath& InClassAssetPath) = 0;
};

}
