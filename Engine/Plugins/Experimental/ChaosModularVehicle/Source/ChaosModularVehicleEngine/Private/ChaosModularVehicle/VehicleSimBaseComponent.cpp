// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosModularVehicle/VehicleSimBaseComponent.h"


UVehicleSimBaseComponent::UVehicleSimBaseComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	BoneName = NAME_None;
	AnimationOffset = FVector::ZeroVector;
	bAnimationEnabled = false;
	AnimationSetupIndex = -1;
	TreeIndex = -1;
}

