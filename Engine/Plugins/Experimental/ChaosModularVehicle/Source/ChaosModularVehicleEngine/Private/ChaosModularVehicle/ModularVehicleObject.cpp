// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosModularVehicle/ModularVehicleObject.h"


UModularVehicle::UModularVehicle(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// want regular mass, mass as density yields considerably larger values which don't make sense for vehicles
	bMassAsDensity = false;

	// default mass value
	Mass = 1500.0f;
}
