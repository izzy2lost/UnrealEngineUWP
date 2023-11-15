// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosModularVehicle/ActorFactoryModularVehicle.h"

#include "ChaosModularVehicle/ModularVehiclePawn.h"
#include "ChaosModularVehicle/ModularVehicleAsset.h"
#include "ChaosModularVehicle/ModularVehicleComponent.h"
#include "ChaosModularVehicle/ModularVehicleAssetFactory.h"
#include "Misc/FileHelper.h"
#include "AssetRegistry/AssetData.h"
#include "Editor.h"
#include "EditorFramework/AssetImportData.h"

#define LOCTEXT_NAMESPACE "ActorFactoryModularVehicle"

DEFINE_LOG_CATEGORY_STATIC(LogChaosModularVehicleFactories, Log, All);

/*-----------------------------------------------------------------------------
UActorFactoryModularVehicle
-----------------------------------------------------------------------------*/
UActorFactoryModularVehicle::UActorFactoryModularVehicle(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DisplayName = LOCTEXT("ModularVehicleDisplayName", "ModularVehicle");
	NewActorClass = AModularVehiclePawn::StaticClass();
}

bool UActorFactoryModularVehicle::CanCreateActorFrom(const FAssetData& AssetData, FText& OutErrorMsg)
{
	if (!AssetData.IsValid() || !AssetData.GetClass()->IsChildOf(UModularVehicleAsset::StaticClass()))
	{
		OutErrorMsg = NSLOCTEXT("CanCreateActor", "NoModularVehicleSpecified", "No ModularVehicle asset was specified.");
		return false;
	}

	return true;
}

void UActorFactoryModularVehicle::PostSpawnActor(UObject* Asset, AActor* NewActor)
{
	Super::PostSpawnActor(Asset, NewActor);

	UModularVehicleAsset* ModularVehicle = CastChecked<UModularVehicleAsset>(Asset);
	AModularVehiclePawn* NewModularVehicleActor = CastChecked<AModularVehiclePawn>(NewActor);

	// Term Component
	NewModularVehicleActor->GetModularVehicleComponent()->UnregisterComponent();

	// Init Component
	NewModularVehicleActor->GetModularVehicleComponent()->RegisterComponent();
}

#undef LOCTEXT_NAMESPACE