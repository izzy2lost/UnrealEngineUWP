// Copyright Epic Games, Inc. All Rights Reserved.

#include "MultiUserReplicationEditorModule.h"

#include "ConcertSyncSessionFlags.h"
#include "IMultiUserClientModule.h"

#include "ISettingsModule.h"
#include "MultiUserReplicationEditorStyle.h"
#include "Settings/MultiUserReplicationSettings.h"
#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "FMultiUserReplicationEditorModule"

namespace UE::MultiUserReplicationEditor
{
	FMultiUserReplicationEditorModule::FMultiUserReplicationEditorModule()
		: MultiUserReplicationCategory(LOCTEXT("MultiUserReplicationCategory", "Multi-User"))
	{}

	void FMultiUserReplicationEditorModule::StartupModule()
	{
		FMultiUserReplicationEditorStyle::Initialize();
		RegisterSettings();
	}

	void FMultiUserReplicationEditorModule::ShutdownModule()
	{
		FMultiUserReplicationEditorStyle::Shutdown();
		UnregisterSettings();
	}

	void FMultiUserReplicationEditorModule::RegisterSettings()
	{
		ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");
		if (ensure(SettingsModule))
		{
			SettingsModule->RegisterSettings("Project", "Plugins", "Multi-User Replication",
				LOCTEXT("MultiUserReplicationSettingsName", "Multi-User Replication"),
				LOCTEXT("MultiUserReplicationSettingsDescription", "Configure the Multi-User Replication settings."),
				UMultiUserReplicationSettings::Get()
			);
		}
	}

	void FMultiUserReplicationEditorModule::UnregisterSettings()
	{
		ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");
		if (SettingsModule)
		{
			SettingsModule->UnregisterSettings("Project", "Plugins", "Multi-User Replication");
		}
	}
};

IMPLEMENT_MODULE(UE::MultiUserReplicationEditor::FMultiUserReplicationEditorModule, MultiUserReplicationEditor);
#undef LOCTEXT_NAMESPACE