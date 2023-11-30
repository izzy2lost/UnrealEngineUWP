// Copyright Epic Games, Inc. All Rights Reserved.

#include "Module.h"
#include "VirtualScoutingLog.h"
#include "VirtualScoutingSettings.h"
#include "ISettingsModule.h"
#include "ISettingsSection.h"


#define LOCTEXT_NAMESPACE "FVirtualScoutingModule"


DEFINE_LOG_CATEGORY(LogVirtualScouting);


void FVirtualScoutingModule::StartupModule()
{
	//RegisterSettings();
}


void FVirtualScoutingModule::ShutdownModule()
{
	//UnregisterSettings();
}


void FVirtualScoutingModule::RegisterSettings() const
{
	ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");
	if (SettingsModule != nullptr)
	{
		ISettingsSectionPtr SettingsSection = SettingsModule->RegisterSettings("Project", "Plugins", "VirtualScouting",
			LOCTEXT("VirtualScoutingSettingsName", "Virtual Scouting"),
			LOCTEXT("VirtualScoutingSettingsDescription", "Configure the Virtual Scouting settings."),
			GetMutableDefault<UVirtualScoutingSettings>());
	}
}


void FVirtualScoutingModule::UnregisterSettings() const
{
	ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");
	if (SettingsModule != nullptr)
	{
		SettingsModule->UnregisterSettings("Project", "Plugins", "VirtualScouting");
	}
}


bool FVirtualScoutingModule::OnSettingsModified()
{
	return true;
}


IMPLEMENT_MODULE(FVirtualScoutingModule, VirtualScouting)


#undef LOCTEXT_NAMESPACE
