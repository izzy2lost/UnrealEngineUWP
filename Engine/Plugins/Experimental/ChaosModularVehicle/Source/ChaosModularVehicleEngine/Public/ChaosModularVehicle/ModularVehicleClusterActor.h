// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ObjectPtr.h"

#include "ModularVehicleClusterActor.generated.h"

class UClusterUnionComponent;
class UModularVehicleBaseComponent;

UCLASS()
class CHAOSMODULARVEHICLEENGINE_API AModularVehicleClusterActor: public AActor
{
	GENERATED_UCLASS_BODY()

public:

	/* VehicleSpecificClusterComponent */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Vehicle, meta = (ExposeFunctionCategories = "Components|ModularVehicle", AllowPrivateAccess = "true"))
	TObjectPtr<UClusterUnionComponent> ClusterUnionComponent;

	/* ModularVehicleComponent */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Vehicle, meta = (ExposeFunctionCategories = "Components|ModularVehicle", AllowPrivateAccess = "true"))
	TObjectPtr<UModularVehicleBaseComponent> VehicleSimComponent;

};
