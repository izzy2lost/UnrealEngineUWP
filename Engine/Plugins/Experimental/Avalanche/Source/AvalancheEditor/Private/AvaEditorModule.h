// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AssetTypeCategories.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "Toolkits/AssetEditorToolkit.h"

class FAvaOutlinerItem;
class IAssetTypeActions;
class IAvaEditor;

DECLARE_LOG_CATEGORY_EXTERN(AvaLog, Log, All);

/**
 * Main Avalanche Editor Module
 */
class FAvaEditorModule : public IModuleInterface
{
public:
	//~ Begin IAvaEditorModule
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	//~ End IAvaEditorModule

	static FSlateIcon GetOutlinerShapeActorIcon(TSharedPtr<const FAvaOutlinerItem> InItem);
	
private:
	/** Avalanche for the Level Editor*/
	TSharedPtr<IAvaEditor> AvaLevelEditor;

	/** All created asset type actions.  Cached here so that we can unregister it during shutdown. */
	TArray<TSharedPtr<IAssetTypeActions>> AssetTypeActions;

	void CreateAvaLevelEditor();

	void PostEngineInit();
	void PreExit();

	void RegisterAssetTools();
	void UnregisterAssetTools();

	void RegisterPropertyEditorCategories();
	
	void RegisterCustomLayouts();
	void UnregisterCustomLayouts();

	void RegisterLevelTemplates();
};
