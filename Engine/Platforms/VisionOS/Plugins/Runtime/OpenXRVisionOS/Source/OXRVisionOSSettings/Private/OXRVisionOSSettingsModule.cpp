// Copyright Epic Games, Inc. All Rights Reserved.

#include "OXRVisionOSSettingsModule.h"
#include "OXRVisionOSRuntimeSettings.h"
#include "Modules/ModuleManager.h"
#include "UObject/WeakObjectPtr.h"
#include "UObject/Class.h"

// Settings
#include "ISettingsModule.h"

#define LOCTEXT_NAMESPACE "OXRVisionOSSettings"

//////////////////////////////////////////////////////////////////////////
// FOXRVisionOSSettings

class FOXRVisionOSSettings : public IOXRVisionOSSettingsModule
{
public:
	virtual void StartupModule() override
	{				
		RegisterSettings();
	}

	virtual void ShutdownModule() override
	{
		if (UObjectInitialized())
		{
			UnregisterSettings();  
		}		
	}
private:
	
	void RegisterSettings()
	{
		if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
		{
			SettingsModule->RegisterSettings("Project", "Plugins", "OXRVisionOS",
				LOCTEXT("RuntimeSettingsName", "OXRVisionOS"),
				LOCTEXT("RuntimeSettingsDescription", "Configure the OXRVisionOS plugin"),
				GetMutableDefault<UOXRVisionOSRuntimeSettings>()
			);
		}
	}

	void UnregisterSettings()
	{
		if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
		{
			SettingsModule->UnregisterSettings("Project", "Plugins", "OXRVisionOS");
		}
	}
};

//////////////////////////////////////////////////////////////////////////

IMPLEMENT_MODULE(FOXRVisionOSSettings, OXRVisionOSSettings);

//////////////////////////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
