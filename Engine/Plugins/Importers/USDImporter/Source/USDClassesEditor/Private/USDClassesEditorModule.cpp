// Copyright Epic Games, Inc. All Rights Reserved.

#include "USDClassesEditorModule.h"

#include "USDAssetCache2.h"
#include "USDAssetCacheAssetActions.h"

#include "AssetToolsModule.h"
#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "USDClassesEditorModule"

UUsdAssetCache2* IUsdClassesEditorModule::ShowMissingDefaultAssetCacheDialog()
{
	return nullptr;
}

void IUsdClassesEditorModule::ShowMissingDefaultAssetCacheDialog(UUsdAssetCache2*& OutCreatedCache, bool& bOutUserAccepted)
{
}

EDefaultAssetCacheDialogOption IUsdClassesEditorModule::ShowMissingDefaultAssetCacheDialog(UUsdAssetCache2*& OutCreatedCache)
{
	return EDefaultAssetCacheDialogOption::Cancel;
}

class FUsdClassesEditorModule : public IUsdClassesEditorModule
{
public:
	virtual void StartupModule() override
	{
		// Register asset actions for the AssetCache asset
		IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();
		AssetCacheAssetActions = MakeShared<FUsdAssetCacheAssetActions>();
		AssetTools.RegisterAssetTypeActions(AssetCacheAssetActions.ToSharedRef());
	}

	virtual void ShutdownModule() override
	{
		// Unregister asset actions for the AssetCache asset
		if (FAssetToolsModule* AssetToolsModule = FModuleManager::GetModulePtr<FAssetToolsModule>(TEXT("AssetTools")))
		{
			IAssetTools& AssetTools = AssetToolsModule->Get();
			AssetTools.UnregisterAssetTypeActions(AssetCacheAssetActions.ToSharedRef());
		}
	}

private:
	TSharedPtr<IAssetTypeActions> AssetCacheAssetActions;
};

IMPLEMENT_MODULE(FUsdClassesEditorModule, USDClassesEditor);

#undef LOCTEXT_NAMESPACE
