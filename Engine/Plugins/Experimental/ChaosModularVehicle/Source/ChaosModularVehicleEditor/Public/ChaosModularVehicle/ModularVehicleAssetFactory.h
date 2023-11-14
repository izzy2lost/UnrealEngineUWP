// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Factories/Factory.h"

#include "ModularVehicleAssetFactory.generated.h"

class UModularVehicleAsset;
class UModularVehicleComponent;

typedef TTuple<const UModularVehicleAsset *, const UModularVehicleComponent *, FTransform> ModularVehicleTuple;


/**
* Factory for ModularVehicleAsset
*/

UCLASS()
class CHAOSMODULARVEHICLEEDITOR_API UModularVehicleAssetFactory : public UFactory
{
    GENERATED_UCLASS_BODY()

	//~ Begin UFactory Interface
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
	//~ Begin UFactory Interface	

	static UModularVehicleAsset* StaticFactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn);

};


