// Copyright Epic Games, Inc. All Rights Reserved.

#include "TypedElementsDataStorageAssetDataModule.h"

#include "Elements/Framework/TypedElementRegistry.h"
#include "HAL/IConsoleManager.h"
#include "Modules/ModuleManager.h"
#include "TedsAssetData.h"

IMPLEMENT_MODULE(UE::TypedElementsDataStorageAssetData::FTypedElementsDataStorageAssetDataModule, TypedElementsDataStorageAssetData);

namespace UE::TypedElementsDataStorageAssetData
{

namespace Private
{
TAutoConsoleVariable<bool> CVarTEDSAssetDataStorage(TEXT("Teds.AssetDataStorage"), false, TEXT("When true we will activate a wrapper that store the a copy of the asset data including the in memory change from the asset registry into TEDS.")
	, FConsoleVariableDelegate::CreateLambda([](IConsoleVariable* Variable)
	{
		const bool bIsEnabled = Variable->GetBool();
		FTypedElementsDataStorageAssetDataModule& Module = FTypedElementsDataStorageAssetDataModule::GetChecked();

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

void FTypedElementsDataStorageAssetDataModule::StartupModule()
{
	if (Private::CVarTEDSAssetDataStorage.GetValueOnGameThread())
	{
		EnableTedsAssetRegistryStorage();
	}
}

void FTypedElementsDataStorageAssetDataModule::ShutdownModule()
{
	if (UTypedElementRegistry* TypedElementRegistry = UTypedElementRegistry::GetInstance())
	{
		TypedElementRegistry->OnDataStorageInterfacesSet().RemoveAll(this);
	}
}

FTypedElementsDataStorageAssetDataModule* FTypedElementsDataStorageAssetDataModule::Get()
{
	return FModuleManager::Get().LoadModulePtr<FTypedElementsDataStorageAssetDataModule>(TEXT("TypedElementsDataStorageAssetData"));
}

FTypedElementsDataStorageAssetDataModule& FTypedElementsDataStorageAssetDataModule::GetChecked()
{
	return FModuleManager::Get().LoadModuleChecked<FTypedElementsDataStorageAssetDataModule>(TEXT("TypedElementsDataStorageAssetData"));
}

void FTypedElementsDataStorageAssetDataModule::EnableTedsAssetRegistryStorage()
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
			TypedElementRegistry->OnDataStorageInterfacesSet().AddRaw(this, &FTypedElementsDataStorageAssetDataModule::InitAssetRegistryStorage);
		}

		if (!Private::CVarTEDSAssetDataStorage.GetValueOnGameThread())
		{
			Private::CVarTEDSAssetDataStorage.AsVariable()->Set(true);
		}
	}
}

void FTypedElementsDataStorageAssetDataModule::DisableTedsAssetRegistryStorage()
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

bool FTypedElementsDataStorageAssetDataModule::IsTedsAssetRegistryStorageEnabled() const
{
	return AssetRegistryStorage.IsValid();
}

void FTypedElementsDataStorageAssetDataModule::ProcessDependentEvents()
{
	if (Private::FTedsAssetData* Storage = AssetRegistryStorage.Get())
	{
		Storage->ProcessAllEvents();
	}
}

void FTypedElementsDataStorageAssetDataModule::InitAssetRegistryStorage()
{
	AssetRegistryStorage = MakeUnique<Private::FTedsAssetData>(*UTypedElementRegistry::GetInstance()->GetMutableDataStorage());
}

}