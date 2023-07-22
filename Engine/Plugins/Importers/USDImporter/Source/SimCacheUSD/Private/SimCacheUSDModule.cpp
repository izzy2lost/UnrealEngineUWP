// Copyright Epic Games, Inc. All Rights Reserved.

#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"

#include "UnrealUSDWrapper.h"
#include "USDMemory.h"


class FSimCacheUSDModule : public IModuleInterface
{
	//~ Begin IModuleInterface API
	virtual void StartupModule() override;
	//~ End IModuleInterface API

};

void FSimCacheUSDModule::StartupModule()
{
#if USE_USD_SDK
	// Register the SimCacheUSD plugin with USD.
	IPluginManager& UEPluginManager = IPluginManager::Get();
	FString USDImporterDir = UEPluginManager.FindPlugin(TEXT("USDImporter"))->GetBaseDir();

	const FString SimCacheUSDResourcesDir =
		FPaths::ConvertRelativePathToFull(
			FPaths::Combine(
				USDImporterDir, 
				FString(TEXT("SimCacheUSD")),
				FString(TEXT("Resources"))));

	UnrealUSDWrapper::RegisterPlugins(SimCacheUSDResourcesDir);
#endif // USE_USD_SDK
}


IMPLEMENT_MODULE_USD(FSimCacheUSDModule, SimCacheUSD);
