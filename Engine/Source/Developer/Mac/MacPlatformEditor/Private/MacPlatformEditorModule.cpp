// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "ISettingsModule.h"
#include "Modules/ModuleManager.h"
#include "UObject/WeakObjectPtr.h"
#include "UObject/Class.h"
#include "PropertyEditorModule.h"
#include "XcodeProjectSettingsDetailsCustomization.h"

#define LOCTEXT_NAMESPACE "MacPlatformEditorModule"

/**
 * Module for Mac project settings
 */
class FMacPlatformEditorModule
	: public IModuleInterface
{
	// IModuleInterface interface

	virtual void StartupModule() override
	{

	}

	virtual void ShutdownModule() override
	{

	}
};


IMPLEMENT_MODULE(FMacPlatformEditorModule, MacPlatformEditor);

#undef LOCTEXT_NAMESPACE
