// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosModularVehicle/ModularVehiclePawn.h"


AModularVehiclePawn::AModularVehiclePawn(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ModularVehicleComponent = CreateDefaultSubobject<UModularVehicleComponent>(TEXT("ModularVehicleComponent0"));

	RootComponent = ModularVehicleComponent;

	PrimaryActorTick.bCanEverTick = true;
	SetActorTickEnabled(true);

	bReplicates = true;
}

void AModularVehiclePawn::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	ModularVehicleComponent->CreateVehicleSim();
}


void AModularVehiclePawn::BeginPlay()
{
	// COPIED THIS FROM STATIC MESH ACTOR
	// BEGIN..
	// Since we allow AStaticMeshActor to specify whether it replicates via bStaticMeshReplicateMovement - per placed instance
	// and we do the normal SetReplicates call in PostInitProperties, before instanced properties are serialized in, 
	// we need to do this here. 
	//
	// This is a short term fix until we find a better play for SetReplicates to be called in AActor.

	if (GetLocalRole() == ROLE_Authority && true/*bStaticMeshReplicateMovement*/)
	{
		bReplicates = false;
		SetRemoteRoleForBackwardsCompat(ROLE_SimulatedProxy);
		SetReplicatingMovement(true); //??

		SetReplicates(true);
	}

	// ..END
	Super::BeginPlay();
}


void AModularVehiclePawn::TickSimulation(float DeltaTime)
{
//	Evolution->AdvanceOneTimeStep(DeltaTime);
}


void AModularVehiclePawn::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (ModularVehicleComponent)
	{
		ModularVehicleComponent->SetRenderStateDirty();
	}
}


