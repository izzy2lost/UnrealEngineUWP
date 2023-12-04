// Copyright Epic Games, Inc. All Rights Reserved.

#include "NNERuntimeIREEModule.h"

#ifdef WITH_NNE_RUNTIME_IREE

#include "NNE.h"
#include "NNERuntime.h"

#ifdef WITH_EDITOR
#include "Interfaces/ITargetPlatformManagerModule.h"
#include "Interfaces/ITargetPlatform.h"
#include "Misc/App.h"
#include "Misc/ConfigCacheIni.h"
#endif // WITH_EDITOR

#endif // WITH_NNE_RUNTIME_IREE

void FNNERuntimeIREEModule::StartupModule()
{
#ifdef WITH_NNE_RUNTIME_IREE

#ifdef WITH_EDITOR
	ITargetPlatformManagerModule* TargetPlatformManagerModule = GetTargetPlatformManager();
	if (TargetPlatformManagerModule)
	{
		TSet<FString> ProcessedPlatforms;
		TArray<ITargetPlatform*> TargetPlatforms = TargetPlatformManagerModule->GetTargetPlatforms();
		for (int32 i = 0; i < TargetPlatforms.Num(); i++)
		{
			FString IniPlatformName = TargetPlatforms[i]->IniPlatformName();
			if (!ProcessedPlatforms.Contains(IniPlatformName))
			{
				FString TargetPlatformDisplayName = IniPlatformName;
				FString TargetPlatformName = IniPlatformName.Equals("Windows") ? "Win64" : IniPlatformName;

				FString ConfigFolderPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectConfigDir());
				FString ConfigFilePath = FPaths::Combine(ConfigFolderPath, TargetPlatformDisplayName, TargetPlatformDisplayName + "Game.ini");
				FConfigFile OnDiskConfigFile;
				OnDiskConfigFile.Read(ConfigFilePath);

				FString CookingPath = FString("/Saved/Cooked/") + TargetPlatformDisplayName + FString("/Engine/Plugins/") + UE_PLUGIN_NAME + FString("/Binaries");
				FString PackagingPath = FString("/Binaries/") + TargetPlatformName + FString("/") + UE_PLUGIN_NAME;

				OnDiskConfigFile.AddUniqueToSection(TEXT("/Script/UnrealEd.ProjectPackagingSettings"), TEXT("+DirectoriesToAlwaysStageAsNonUFS"), FString("(Path=\"..") + CookingPath + FString("\")"));
				OnDiskConfigFile.AddUniqueToSection(TEXT("Staging"), TEXT("+RemapDirectories"), FString("(From=\"") + FApp::GetProjectName() + CookingPath + FString("\", To=\"") + FApp::GetProjectName() + PackagingPath + FString("\")"));
				OnDiskConfigFile.AddUniqueToSection(TEXT("Staging"), TEXT("+AllowedDirectories"), FApp::GetProjectName() + PackagingPath);

				// Write returns true without an attempt to write if the config has not been marked dirty and AddUniqueToSection will only mark it dirty if there are changes
				// Thus the warning will not be printed when a locked file contains the required settings
				if (!OnDiskConfigFile.Write(ConfigFilePath))
				{
					UE_LOG(LogTemp, Warning, TEXT("UNNERuntimeIREECpu could not write config file %s. Please make the file writeable and re-cook or manually add the required staging settings or models will not work in packaged builds for platform %s!"), *ConfigFilePath, *TargetPlatformDisplayName);
				}

				ProcessedPlatforms.Add(IniPlatformName);
			}
		}
	}
#endif // WITH_EDITOR

	NNERuntimeIREECpu = NewObject<UNNERuntimeIREECpu>();
	if (NNERuntimeIREECpu.IsValid())
	{
		NNERuntimeIREECpu->AddToRoot();
		UE::NNE::RegisterRuntime(NNERuntimeIREECpu.Get());
	}

	NNERuntimeIREECuda = NewObject<UNNERuntimeIREECuda>();
	if (NNERuntimeIREECuda.IsValid())
	{
		if (NNERuntimeIREECuda->IsAvailable())
		{
			NNERuntimeIREECuda->AddToRoot();
			UE::NNE::RegisterRuntime(NNERuntimeIREECuda.Get());
		}
		else
		{
			NNERuntimeIREECuda.Reset();
		}
	}

	NNERuntimeIREEVulkan = NewObject<UNNERuntimeIREEVulkan>();
	if (NNERuntimeIREEVulkan.IsValid())
	{
		if (NNERuntimeIREEVulkan->IsAvailable())
		{
			NNERuntimeIREEVulkan->AddToRoot();
			UE::NNE::RegisterRuntime(NNERuntimeIREEVulkan.Get());
		}
		else
		{
			NNERuntimeIREEVulkan.Reset();
		}
	}

	NNERuntimeIREERdg = NewObject<UNNERuntimeIREERdg>();
	if (NNERuntimeIREERdg.IsValid())
	{
		if (NNERuntimeIREERdg->IsAvailable())
		{
			NNERuntimeIREERdg->AddToRoot();
			UE::NNE::RegisterRuntime(NNERuntimeIREERdg.Get());
		}
		else
		{
			NNERuntimeIREERdg.Reset();
		}
	}
#endif // WITH_NNE_RUNTIME_IREE
}

void FNNERuntimeIREEModule::ShutdownModule()
{
#ifdef WITH_NNE_RUNTIME_IREE
	if (NNERuntimeIREECpu.IsValid())
	{
		UE::NNE::UnregisterRuntime(NNERuntimeIREECpu.Get());
		NNERuntimeIREECpu->RemoveFromRoot();
		NNERuntimeIREECpu.Reset();
	}

	if (NNERuntimeIREECuda.IsValid())
	{
		UE::NNE::UnregisterRuntime(NNERuntimeIREECuda.Get());
		NNERuntimeIREECuda->RemoveFromRoot();
		NNERuntimeIREECuda.Reset();
	}

	if (NNERuntimeIREEVulkan.IsValid())
	{
		UE::NNE::UnregisterRuntime(NNERuntimeIREEVulkan.Get());
		NNERuntimeIREEVulkan->RemoveFromRoot();
		NNERuntimeIREEVulkan.Reset();
	}

	if (NNERuntimeIREERdg.IsValid())
	{
		UE::NNE::UnregisterRuntime(NNERuntimeIREERdg.Get());
		NNERuntimeIREERdg->RemoveFromRoot();
		NNERuntimeIREERdg.Reset();
	}
#endif // WITH_NNE_RUNTIME_IREE
}
	
IMPLEMENT_MODULE(FNNERuntimeIREEModule, NNERuntimeIREE)