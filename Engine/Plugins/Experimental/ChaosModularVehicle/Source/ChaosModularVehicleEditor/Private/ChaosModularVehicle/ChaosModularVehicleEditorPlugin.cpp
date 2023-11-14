// Copyright Epic Games, Inc. All Rights Reserved.


#include "ChaosModularVehicle/ChaosModularVehicleEditorPlugin.h"

#include "AssetToolsModule.h"
#include "ChaosModularVehicle/ModularVehicleComponent.h"
#include "ChaosModularVehicle/ModularVehicleAssetConversion.h"
#include "ChaosModularVehicle/ChaosModularVehicleCommands.h"
#include "ChaosModularVehicle/AssetTypeActions_ModularVehicleAsset.h"
#include "ChaosModularVehicle/ModularVehicleAssetThumbnailRenderer.h"
#include "ChaosModularVehicle/SimModuleActor.h"

#include "CoreMinimal.h"
#include "LevelEditor.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "HAL/ConsoleManager.h"
#include "Features/IModularFeatures.h"
#include "Styling/SlateStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "Styling/CoreStyle.h"
#include "ToolMenus.h"


#define BOX_BRUSH(StyleSet, RelativePath, ...) FSlateBoxBrush(StyleSet->RootToContentDir(RelativePath, TEXT(".png")), __VA_ARGS__)
#define DEFAULT_FONT(...) FCoreStyle::GetDefaultFontStyle(__VA_ARGS__)

class FChaosModularVehicleEditorPlugin : public IChaosModularVehicleEditorPlugin
{
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	//void OnPostWorldInitialization(UWorld* InWorld, const UWorld::InitializationValues);

};

IMPLEMENT_MODULE(FChaosModularVehicleEditorPlugin, ModularVehicleAssetEditor);


void FChaosModularVehicleEditorPlugin::StartupModule()
{
	FAssetToolsModule& AssetToolsModule = FAssetToolsModule::GetModule();
	IAssetTools& AssetTools = AssetToolsModule.Get();
	ModularVehicleAssetActions = new FAssetTypeActions_ModularVehicleAsset();
	AssetTools.RegisterAssetTypeActions(MakeShareable(ModularVehicleAssetActions));

	if (GIsEditor && !IsRunningCommandlet())
	{

	}

	UThumbnailManager::Get().RegisterCustomRenderer(UModularVehicleAsset::StaticClass(), UModularVehicleAssetThumbnailRenderer::StaticClass());

	//OnPostWorldInitializationHandle = FWorldDelegates::OnPostWorldInitialization.AddRaw(this, &FChaosModularVehicleEditorPlugin::OnPostWorldInitialization);

}


void FChaosModularVehicleEditorPlugin::ShutdownModule()
{
	if (UObjectInitialized())
	{	
		UThumbnailManager::Get().UnregisterCustomRenderer(UModularVehicleAsset::StaticClass());

		FAssetToolsModule& AssetToolsModule = FAssetToolsModule::GetModule();
		IAssetTools& AssetTools = AssetToolsModule.Get();
		AssetTools.UnregisterAssetTypeActions(ModularVehicleAssetActions->AsShared());

		//FWorldDelegates::OnPostWorldInitialization.Remove(OnPostWorldInitializationHandle);
	}
}
