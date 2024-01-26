// Copyright Epic Games, Inc. All Rights Reserved.

#include "AdvancedRenamerModule.h"
#include "AdvancedRenamerCommands.h"
#include "AdvancedRenamerStyle.h"
#include "Integrations/AdvancedRenamerContentBrowserIntegration.h"
#include "Integrations/AdvancedRenamerLevelEditorIntegration.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY(LogARP);

FAdvancedRenamerModule& FAdvancedRenamerModule::Get()
{
	static const FName ModuleName = TEXT("AdvancedRenamer");
	return FModuleManager::LoadModuleChecked<FAdvancedRenamerModule>(ModuleName);
}

void FAdvancedRenamerModule::StartupModule()
{
	FAdvancedRenamerStyle::Initialize();
	FAdvancedRenamerCommands::Register();
	FAdvancedRenamerContentBrowserIntegration::Initialize();
	FAdvancedRenamerLevelEditorIntegration::Initialize();
}

void FAdvancedRenamerModule::ShutdownModule()
{
	FAdvancedRenamerCommands::Unregister();
	FAdvancedRenamerStyle::Shutdown();
	FAdvancedRenamerContentBrowserIntegration::Shutdown();
	FAdvancedRenamerLevelEditorIntegration::Shutdown();
}

IMPLEMENT_MODULE(FAdvancedRenamerModule, AdvancedRenamer)
