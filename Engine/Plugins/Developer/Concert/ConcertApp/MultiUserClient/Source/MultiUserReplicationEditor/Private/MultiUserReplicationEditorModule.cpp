// Copyright Epic Games, Inc. All Rights Reserved.

#include "MultiUserReplicationEditorModule.h"

#include "ConcertSyncSessionFlags.h"
#include "CVarMultiUserReplicationEditor.h"
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

		UpdateSettingsRegistrationBasedOnCVar();
		ConsoleVariables::CVarEnableReplication->OnChangedDelegate().AddLambda([this](IConsoleVariable*)
		{
			UpdateSettingsRegistrationBasedOnCVar();
		});
	}

	void FMultiUserReplicationEditorModule::ShutdownModule()
	{
		FMultiUserReplicationEditorStyle::Shutdown();
	}

	void FMultiUserReplicationEditorModule::UpdateSettingsRegistrationBasedOnCVar()
	{
		if (ConsoleVariables::CVarEnableReplication.GetValueOnAnyThread())
		{
			RegisterSettings();
		}
		else
		{
			UnregisterSettings();
		}
	}

	void FMultiUserReplicationEditorModule::RegisterSettings()
	{
		ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");
		SettingsModule->RegisterSettings("Project", "Plugins", "Multi-User Replication",
			LOCTEXT("MultiUserReplicationSettingsName", "Multi-User Replication"),
			LOCTEXT("MultiUserReplicationSettingsDescription", "Configure the Multi-User Replication settings."),
			UMultiUserReplicationSettings::Get()
		);
	}

	void FMultiUserReplicationEditorModule::UnregisterSettings()
	{
		ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");
		SettingsModule->UnregisterSettings("Project", "Plugins", "Multi-User Replication");
	}
};

IMPLEMENT_MODULE(UE::MultiUserReplicationEditor::FMultiUserReplicationEditorModule, MultiUserReplicationEditor);
#undef LOCTEXT_NAMESPACE