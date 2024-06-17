// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleInterface.h"
#include "Templates/UniquePtr.h"

namespace UE::TypedElementsDataStorageAssetData
{ 

namespace Private
{
	class FTedsAssetData;
}

class FTypedElementsDataStorageAssetDataModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	static FTypedElementsDataStorageAssetDataModule* Get();
	static FTypedElementsDataStorageAssetDataModule& GetChecked();

	void EnableTedsAssetRegistryStorage();
	void DisableTedsAssetRegistryStorage();
	bool IsTedsAssetRegistryStorageEnabled() const;


	/**
	 * Process now any pending event that might make the Teds database out of sync with the asset registry.
	 * Note: This isn't needed when using the editor so it should be only called by the automation scripts that need it to avoid creating some unneeded stalls.
	 */
	void ProcessDependentEvents();

private:
	void InitAssetRegistryStorage();

	TUniquePtr<Private::FTedsAssetData> AssetRegistryStorage;
};

}
