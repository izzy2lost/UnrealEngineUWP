// Copyright Epic Games, Inc. All Rights Reserved.

#include "Actors/ChaosVDSolverInfoActor.h"

#include "ChaosVDParticleActor.h"
#include "ChaosVDScene.h"
#include "Components/ChaosVDSolverCollisionDataComponent.h"
#include "Elements/Framework/TypedElementSelectionSet.h"
#include "Engine/World.h"

AChaosVDSolverInfoActor::AChaosVDSolverInfoActor(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	CollisionDataComponent = CreateDefaultSubobject<UChaosVDSolverCollisionDataComponent>(TEXT("SolverCollisionDataComponent"));
}

void AChaosVDSolverInfoActor::SetScene(TWeakPtr<FChaosVDScene> InScene)
{
	FChaosVDSceneObjectBase::SetScene(InScene);

	if (TSharedPtr<FChaosVDScene> ScenePtr = InScene.Pin())
	{
		RegisterSelectionSetObject(ScenePtr->GetElementSelectionSet());
	}
}

void AChaosVDSolverInfoActor::RegisterParticleActor(int32 ParticleID, AChaosVDParticleActor* ParticleActor)
{
	if (!SolverParticlesByID.Contains(ParticleID))
	{
		SolverParticlesByID.Add(ParticleID, ParticleActor);
	}
}

AChaosVDParticleActor* AChaosVDSolverInfoActor::GetParticleActor(int32 ParticleID)
{
	AChaosVDParticleActor** FoundParticleActor = SolverParticlesByID.Find(ParticleID);

	return FoundParticleActor ? *FoundParticleActor : nullptr;
}

void AChaosVDSolverInfoActor::HandleVisibilitySettingsUpdated()
{
	for (const TPair<int32, AChaosVDParticleActor*>& ParticleWithIDPair : SolverParticlesByID)
	{
		if (AChaosVDParticleActor* ParticleActor = ParticleWithIDPair.Value)
		{
			ParticleActor->UpdateGeometryComponentsVisibility();
		}
	}
}

void AChaosVDSolverInfoActor::HandleColorsSettingsUpdated()
{
	for (const TPair<int32, AChaosVDParticleActor*>& ParticleWithIDPair : SolverParticlesByID)
	{
		if (AChaosVDParticleActor* ParticleActor = ParticleWithIDPair.Value)
		{
			ParticleActor->UpdateGeometryColors();
		}	
	}
}

void AChaosVDSolverInfoActor::BeginDestroy()
{
	Super::BeginDestroy();

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	
	for (const TPair<int32, AChaosVDParticleActor*>& ParticleVDInstanceWithID : SolverParticlesByID)
	{
		World->DestroyActor(ParticleVDInstanceWithID.Value);
	}
}

void AChaosVDSolverInfoActor::HandlePostSelectionChange(const UTypedElementSelectionSet* ChangesSelectionSet)
{
	SelectedParticlesID.Reset();

	TArray<AChaosVDParticleActor*> SelectedParticles = ChangesSelectionSet->GetSelectedObjects<AChaosVDParticleActor>();

	//TODO: Support multi-selection
	if (SelectedParticles.Num() > 0)
	{
		if (AChaosVDParticleActor* SelectedParticle = SelectedParticles[0])
		{
			if (const FChaosVDParticleDataWrapper* ParticleData = SelectedParticle->GetParticleData())
			{
				SelectedParticlesID.Add(ParticleData->ParticleIndex);
			}
		}
	}
}
