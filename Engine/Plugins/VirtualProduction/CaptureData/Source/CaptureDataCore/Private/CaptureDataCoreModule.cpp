// Copyright Epic Games, Inc. All Rights Reserved.

#include "CaptureDataCoreModule.h"

#include "CaptureData.h"
#include "CaptureDataLog.h"

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/MessageDialog.h"
#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "CaptureDataCoreModule"

void FCaptureDataCoreModule::StartupModule()
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	AssetRegistryModule.Get().OnFilesLoaded().AddRaw(this, &FCaptureDataCoreModule::CheckAssetMigration);
}

void FCaptureDataCoreModule::ShutdownModule()
{

}

/*
Check for MetaHuman assets which need to be migrated due to the source code moving to the Capture Data plugin.
*/
void FCaptureDataCoreModule::CheckAssetMigration()
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	TArray<FAssetData> Assets;
	
	// Some MetaHuman assets have moved to the Capture Data plugin so find assets which are referencing the old class path
	FARFilter Filter;
	Filter.ClassPaths.Add(FTopLevelAssetPath(FName("/Script/MetaHumanCaptureData"), FName("FootageCaptureData")));
	Filter.ClassPaths.Add(FTopLevelAssetPath(FName("/Script/MetaHumanCaptureData"), FName("MeshCaptureData")));
	Filter.ClassPaths.Add(FTopLevelAssetPath(FName("/Script/MetaHumanCore"), FName("MetaHumanCameraCalibration")));
	Filter.bIncludeOnlyOnDiskAssets = true;

	AssetRegistryModule.Get().GetAssets(Filter, Assets);

	// Prompt user to reload these assets
	if (!Assets.IsEmpty())
	{
		const FText AssetMigrationDialogMessage = LOCTEXT("CaptureDataAssetMigrationDialog", "Found {0} Capture Data assets which need to be reloaded."
			" Some MetaHuman Animator features may be unavailable until these assets are reloaded. \n\nReload now ? ");
		const EAppReturnType::Type Answer = FMessageDialog::Open(EAppMsgType::YesNo, FText::Format(AssetMigrationDialogMessage, Assets.Num()));
		if (Answer == EAppReturnType::No)
		{
			return;
		}
	}

	for (FAssetData AssetData : Assets)
	{
		AssetData.GetAsset()->ReloadConfig();
	}
}

IMPLEMENT_MODULE(FCaptureDataCoreModule, CaptureDataCore)

#undef LOCTEXT_NAMESPACE