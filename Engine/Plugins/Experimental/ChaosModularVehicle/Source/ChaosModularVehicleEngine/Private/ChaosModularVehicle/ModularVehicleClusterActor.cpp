// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosModularVehicle/ModularVehicleClusterActor.h"

#include "PhysicsEngine/ClusterUnionComponent.h"
#include "ChaosModularVehicle/ModularVehicleBaseComponent.h"


AModularVehicleClusterActor::AModularVehicleClusterActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ClusterUnionComponent = CreateDefaultSubobject<UClusterUnionComponent>(TEXT("ClusterUnionComponent0"));
	SetRootComponent(ClusterUnionComponent);

	// Vehicle sim piggy-backs off of the cluster union component & its events
	VehicleSimComponent = CreateDefaultSubobject<UModularVehicleBaseComponent>(TEXT("VehicleSimComponent0"));
	VehicleSimComponent->SetClusterComponent(ClusterUnionComponent);

	SetRemoteRoleForBackwardsCompat(ROLE_SimulatedProxy);
	bReplicates = true;
	SetReplicatingMovement(true);
}
