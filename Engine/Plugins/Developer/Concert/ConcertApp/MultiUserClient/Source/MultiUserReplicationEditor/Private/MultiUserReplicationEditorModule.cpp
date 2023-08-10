// Copyright Epic Games, Inc. All Rights Reserved.

#include "MultiUserReplicationEditorModule.h"

#include "ConcertSyncSessionFlags.h"

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

	IMultiUserReplicationEditorModule::FSettingPath FMultiUserReplicationEditorModule::GetReplicationSettingsInfo() const
	{
		return { "Project", "Plugins", "Multi-User Replication" };
	}

	void FMultiUserReplicationEditorModule::RegisterSettings()
	{
		ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");
		if (ensure(SettingsModule))
		{
			const FSettingPath Path = GetReplicationSettingsInfo();
			SettingsSection = SettingsModule->RegisterSettings(Path.ContainerName, Path.CategoryName, Path.SectionName,
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
			const FSettingPath Path = GetReplicationSettingsInfo();
			SettingsModule->UnregisterSettings(Path.ContainerName, Path.CategoryName, Path.SectionName);
		}

		SettingsSection = nullptr;
	}
};

IMPLEMENT_MODULE(UE::MultiUserReplicationEditor::FMultiUserReplicationEditorModule, MultiUserReplicationEditor);
#undef LOCTEXT_NAMESPACE