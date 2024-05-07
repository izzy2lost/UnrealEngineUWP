// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class SWidget;

namespace UE::AnimNext::Editor
{

struct FParameterPickerArgs;

class IModule : public IModuleInterface
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
};

}