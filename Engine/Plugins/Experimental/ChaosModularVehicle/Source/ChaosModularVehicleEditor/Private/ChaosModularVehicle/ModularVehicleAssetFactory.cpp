// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosModularVehicle/ModularVehicleAssetFactory.h"

#include "ChaosModularVehicle/ModularVehicleAsset.h"
#include "ChaosModularVehicle/ModularVehicleComponent.h"
#include "Editor.h"
#include "Editor/EditorEngine.h"
#include "Engine/Selection.h"


#define LOCTEXT_NAMESPACE "ChaosModularVehicle"

/////////////////////////////////////////////////////
// ModularVehicleFactory

UModularVehicleAssetFactory::UModularVehicleAssetFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UModularVehicleAsset::StaticClass();
}

UModularVehicleAsset* UModularVehicleAssetFactory::StaticFactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	UModularVehicleAsset* NewModularVehicle = static_cast<UModularVehicleAsset*>(NewObject<UModularVehicleAsset>(InParent, Class, Name, Flags | RF_Transactional | RF_Public | RF_Standalone));
	return NewModularVehicle;
}

UObject* UModularVehicleAssetFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	FTransform LastTransform = FTransform::Identity;
	TArray< ModularVehicleTuple > ModularVehicleList;

	TArray<FAssetData> SelectedAssets;
	GEditor->GetContentBrowserSelections(SelectedAssets);

	for (const FAssetData& AssetData : SelectedAssets)
	{
		if (AssetData.GetAsset()->IsA<UModularVehicleAsset>())
		{
			UModularVehicleComponent *DummyValue(NULL);
			ModularVehicleList.Add(ModularVehicleTuple(static_cast<const UModularVehicleAsset *>(AssetData.GetAsset()), DummyValue, FTransform()));
		}

	}

	UModularVehicleAsset* NewModularVehicle = StaticFactoryCreateNew(Class, InParent, Name, Flags, Context, Warn);

	ensure(false); // @todo(chaos) implement proper copy
	//for (ModularVehicleTuple & ModularVehicleData : ModularVehicleList)
	//{
	//	NewModularVehicle->AppendGeometry(*ModularVehicleData.Get<0>(), false, ModularVehicleData.Get<2>());
	//}

	ensure(false); // @todo(chaos) implement proper simulation prep
	//ModularVehicleAlgo::PrepareForSimulation(NewModularVehicle->GetModularVehicle().Get());

	NewModularVehicle->Modify();
	return NewModularVehicle;
}

#undef LOCTEXT_NAMESPACE



