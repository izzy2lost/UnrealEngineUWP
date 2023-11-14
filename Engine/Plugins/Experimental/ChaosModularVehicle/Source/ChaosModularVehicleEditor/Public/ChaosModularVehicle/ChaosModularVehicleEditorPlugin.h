// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

class ISlateStyle;
class FSlateStyleSet;
class FAssetTypeActions_ModularVehicleAsset;


/**
 * The public interface to this module
 */
class IChaosModularVehicleEditorPlugin : public IModuleInterface
{
public:

	/**
	 * Singleton-like access to this module's interface.  This is just for convenience!
	 * Beware of calling this during the shutdown phase, though.  Your module might have been unloaded already.
	 *
	 * @return Returns singleton instance, loading the module on demand if needed
	 */
	static inline IChaosModularVehicleEditorPlugin& Get()
	{
		return FModuleManager::LoadModuleChecked< IChaosModularVehicleEditorPlugin >( "ChaosModularVehicleEditor" );
	}

	/**
	 * Checks to see if this module is loaded and ready.  It is only valid to call Get() if IsAvailable() returns true.
	 *
	 * @return True if the module is loaded and ready to use
	 */
	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded( "ChaosModularVehicleEditor" );
	}

	TSharedPtr<FSlateStyleSet> GetStyleSet() { return StyleSet; }

	static FName GetEditorStyleName();
	static const ISlateStyle* GetEditorStyle();

protected:
	void RegisterMenus();

	TArray<IConsoleObject*> EditorCommands;

	// Asset actions for new asset types
	FAssetTypeActions_ModularVehicleAsset* ModularVehicleAssetActions;

	// Styleset for flesh tool brushes/fonts etc.
	TSharedPtr<FSlateStyleSet> StyleSet;
};

