// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GeometryCollection/GeometryCollectionObject.h"
#include "ChaosModularVehicle/ModularSimCollection.h"

#include "ModularVehicleObject.generated.h"

namespace Chaos
{
	class FSimModuleTree;
	class ISimulationModuleBase;
}

UCLASS(BlueprintType, customconstructor)
class CHAOSMODULARVEHICLEENGINE_API UModularVehicle : public UGeometryCollection
{
	GENERATED_UCLASS_BODY()
	public:

	UModularVehicle(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

};

