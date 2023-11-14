// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ObjectPtr.h"
#include "ChaosModularVehicle/ModularVehicleComponent.h"

#include "ModularVehicleActor.generated.h"

class ASimModuleActor;
class UGeometryCollectionDebugDrawComponent;


UCLASS()
class CHAOSMODULARVEHICLEENGINE_API AModularVehicleActor: public AActor
{
	GENERATED_UCLASS_BODY()

protected:
	//virtual void BeginPlay() override;

public:

	/* Game state callback */
	void TickSimulation(float DeltaTime);
	virtual void Tick(float DeltaSeconds) override;

	virtual void PostInitializeComponents() override;

	/* ModularVehicleComponent */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Destruction, meta = (ExposeFunctionCategories = "Components|ModularVehicle", AllowPrivateAccess = "true"))
	TObjectPtr<UModularVehicleComponent> ModularVehicleComponent;

	UModularVehicleComponent* GetModularVehicleComponent() const { return ModularVehicleComponent; }

};
