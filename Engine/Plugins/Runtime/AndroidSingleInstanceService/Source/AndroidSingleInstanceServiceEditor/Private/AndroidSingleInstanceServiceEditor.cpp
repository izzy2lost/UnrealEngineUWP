// Copyright Epic Games, Inc. All Rights Reserved.

#include "AndroidSingleInstanceServiceEditor.h"
#include "ISettingsModule.h"
#include "AndroidSingleInstanceServiceRuntimeSettings.h"
#include "Modules/ModuleManager.h"

/**
 * Implements the AndroidSingleInstanceServiceEditor module.
 */

#define LOCTEXT_NAMESPACE "AndroidSingleInstanceService"

void FAndroidSingleInstanceServiceEditorModule::StartupModule()
{
	// register settings
	ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");

	if (SettingsModule != nullptr)	
	{
		SettingsModule->RegisterSettings("Project", "Plugins", "AndroidSingleInstanceService",
			LOCTEXT("AndroidSingleInstanceServiceSettingsName", "AndroidSingleInstanceService"),
			LOCTEXT("AndroidSingleInstanceServiceSettingsDescription", "Project settings for AndroidSingleInstanceService plugin"),
			GetMutableDefault<UAndroidSingleInstanceServiceRuntimeSettings>()
		);
	}
}

void FAndroidSingleInstanceServiceEditorModule::ShutdownModule()
{
	// unregister settings
	ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");

	if (SettingsModule != nullptr)
	{
       		SettingsModule->UnregisterSettings("Project", "Plugins", "AndroidSingleInstanceService");
	}
}


IMPLEMENT_MODULE(FAndroidSingleInstanceServiceEditorModule, AndroidSingleInstanceServiceEditor);

#undef LOCTEXT_NAMESPACE
