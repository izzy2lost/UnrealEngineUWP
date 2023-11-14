// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosModularVehicle/ModularVehicleActor.h"

DEFINE_LOG_CATEGORY_STATIC(AModularVehicleLogging, Log, All);

AModularVehicleActor::AModularVehicleActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ModularVehicleComponent = CreateDefaultSubobject<UModularVehicleComponent>(TEXT("ModularVehicleComponent0"));

	RootComponent = ModularVehicleComponent;

	PrimaryActorTick.bCanEverTick = true;
	SetActorTickEnabled(true);

	bReplicates = true;
}

void AModularVehicleActor::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	ModularVehicleComponent->CreateVehicleSim();
}

void AModularVehicleActor::TickSimulation(float DeltaTime)
{
}

void AModularVehicleActor::Tick(float DeltaTime) 
{
	Super::Tick(DeltaTime);

	if (ModularVehicleComponent)
	{
		ModularVehicleComponent->SetRenderStateDirty();
	}
}

