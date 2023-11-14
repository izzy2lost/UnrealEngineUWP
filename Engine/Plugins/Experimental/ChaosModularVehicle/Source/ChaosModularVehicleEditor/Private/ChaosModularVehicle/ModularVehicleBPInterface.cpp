// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosModularVehicle/ModularVehicleBPInterface.h"
#include "ChaosModularVehicle/ModularToolkitBlueprintLibrary.h"
#include "ChaosModularVehicle/ModularVehiclePawn.h"


UModularVehicleBPInterface::UModularVehicleBPInterface(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UModularVehicleBPInterface::AppendSimModule(AModularVehiclePawn* ExistingActorInOut, int32 ParentIndex, const AActor* Actor, int32& OutIndex)
{
	OutIndex = UModularToolkitBlueprintLibrary::AddChild(ExistingActorInOut, ParentIndex, Actor);
}

void UModularVehicleBPInterface::ConvertToVehicle(const FString& NewAssetPath, const TArray<AActor*>& Actors)
{
	UModularToolkitBlueprintLibrary::ConvertActorsToModularVehicle(NewAssetPath, Actors);
}
