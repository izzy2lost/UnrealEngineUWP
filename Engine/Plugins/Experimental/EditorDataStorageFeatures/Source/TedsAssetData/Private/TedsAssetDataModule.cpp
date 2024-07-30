// Copyright Epic Games, Inc. All Rights Reserved.

#include "TedsAssetDataModule.h"

#include "CB/TedsAssetDataCBDataSource.h"
#include "Elements/Framework/TypedElementRegistry.h"
#include "HAL/IConsoleManager.h"
#include "Modules/ModuleManager.h"
#include "TedsAssetData.h"

IMPLEMENT_MODULE(UE::EditorDataStorage::AssetData::FTedsAssetDataModule, TedsAssetData);

namespace UE::EditorDataStorage::AssetData
{

namespace Private
{
TAutoConsoleVariable<bool> CVarTEDSAssetDataStorage(TEXT("TEDS.AssetDataStorage"), false, TEXT("When true we will activate a wrapper that store the a copy of the asset data including the in memory change from the asset registry into TEDS.")
	, FConsoleVariableDelegate::CreateLambda([](IConsoleVariable* Variable)
	{
		const bool bIsEnabled = Variable->GetBool();
		FTedsAssetDataModule& Module = FTedsAssetDataModule::GetChecked();

		if (bIsEnabled)
		{
			Module.EnableTedsAssetRegistryStorage();
		}
		else
		{
			Module.DisableTedsAssetRegistryStorage();
		}
	}));
}

void FTedsAssetDataModule::StartupModule()
{
	if (Private::CVarTEDSAssetDataStorage.GetValueOnGameThread())
	{
		EnableTedsAssetRegistryStorage();
	}
}

void FTedsAssetDataModule::ShutdownModule()
{
	if (UTypedElementRegistry* TypedElementRegistry = UTypedElementRegistry::GetInstance())
	{
		TypedElementRegistry->OnDataStorageInterfacesSet().RemoveAll(this);
	}
}

FTedsAssetDataModule* FTedsAssetDataModule::Get()
{
	return FModuleManager::Get().LoadModulePtr<FTedsAssetDataModule>(TEXT("TedsAssetData"));
}

FTedsAssetDataModule& FTedsAssetDataModule::GetChecked()
{
	return FModuleManager::Get().LoadModuleChecked<FTedsAssetDataModule>(TEXT("TedsAssetData"));
}

void FTedsAssetDataModule::EnableTedsAssetRegistryStorage()
{
	if (!AssetRegistryStorage)
	{
		FModuleManager::Get().LoadModuleChecked(TEXT("TypedElementFramework"));
		UTypedElementRegistry* TypedElementRegistry = UTypedElementRegistry::GetInstance();
		if (TypedElementRegistry->GetMutableDataStorage())
		{
			InitAssetRegistryStorage();
		}
		else
		{
			TypedElementRegistry->OnDataStorageInterfacesSet().AddRaw(this, &FTedsAssetDataModule::InitAssetRegistryStorage);
		}

		if (!Private::CVarTEDSAssetDataStorage.GetValueOnGameThread())
		{
			Private::CVarTEDSAssetDataStorage.AsVariable()->Set(true);
		}
	}
}

void FTedsAssetDataModule::DisableTedsAssetRegistryStorage()
{
	if (AssetRegistryStorage)
	{
		AssetRegistryStorage.Reset();

		if (Private::CVarTEDSAssetDataStorage.GetValueOnGameThread())
		{
			Private::CVarTEDSAssetDataStorage.AsVariable()->Set(false);
		}
	}
}

bool FTedsAssetDataModule::IsTedsAssetRegistryStorageEnabled() const
{
	return AssetRegistryStorage.IsValid();
}

void FTedsAssetDataModule::ProcessDependentEvents()
{
	if (Private::FTedsAssetData* Storage = AssetRegistryStorage.Get())
	{
		Storage->ProcessAllEvents();
	}
}

void FTedsAssetDataModule::InitAssetRegistryStorage()
{
	ITypedElementDataStorageInterface& MutableDataStorage = *UTypedElementRegistry::GetInstance()->GetMutableDataStorage();

	AssetDataCBDataSource = MakeUnique<Private::FTedsAssetDataCBDataSource>(MutableDataStorage);
	AssetRegistryStorage = MakeUnique<Private::FTedsAssetData>(MutableDataStorage);
}

}