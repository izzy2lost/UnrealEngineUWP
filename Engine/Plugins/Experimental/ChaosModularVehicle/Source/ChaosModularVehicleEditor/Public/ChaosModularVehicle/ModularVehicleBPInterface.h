// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ModularVehicleBPInterface.generated.h"

class AModularVehiclePawn;
class AActor;

// Test to see exposing of BP function - not working in editor portion of module!
UCLASS(ClassGroup = (Physics))
class CHAOSMODULARVEHICLEEDITOR_API UModularVehicleBPInterface : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()

	UFUNCTION(BlueprintCallable, Category = "ModularVehicle")
	static void AppendSimModule(AModularVehiclePawn* ExistingActorInOut, int32 ParentIndex, const AActor* Actor, int32& OutIndex);

	UFUNCTION(BlueprintCallable, Category = "ModularVehicle")
	static void ConvertToVehicle(const FString& NewAssetPath, const TArray<AActor*>& Actors);

	UFUNCTION(BlueprintCallable, Category = "ModularVehicle")
	static float GenerateAsset()
	{
		return false;
	}

};
