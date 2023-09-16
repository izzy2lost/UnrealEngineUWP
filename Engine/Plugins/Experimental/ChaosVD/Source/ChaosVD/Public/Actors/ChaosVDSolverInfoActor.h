// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ChaosVDSceneObjectBase.h"
#include "ChaosVDSceneSelectionObserver.h"
#include "GameFramework/Actor.h"
#include "ChaosVDSolverInfoActor.generated.h"

class AChaosVDParticleActor;
class UChaosVDSolverCollisionDataComponent;

UCLASS()
class AChaosVDSolverInfoActor : public AActor, public FChaosVDSceneObjectBase, public FChaosVDSceneSelectionObserver
{
	GENERATED_BODY()

public:

	AChaosVDSolverInfoActor(const FObjectInitializer& ObjectInitializer);

	void SetSolverID(int32 InSolverID) { SolverID = InSolverID; }
	int32 GetSolverID() const { return SolverID; }

	virtual void SetScene(TWeakPtr<FChaosVDScene> InScene) override;

	void SetSimulationTransform(const FTransform& InSimulationTransform) { SimulationTransform = InSimulationTransform; }
	const FTransform& GetSimulationTransform() const { return SimulationTransform; }

	UChaosVDSolverCollisionDataComponent* GetCollisionDataComponent() { return CollisionDataComponent; }

	void RegisterParticleActor(int32 ParticleID, AChaosVDParticleActor* ParticleActor);

	AChaosVDParticleActor* GetParticleActor(int32 ParticleID);
	const TMap<int32, AChaosVDParticleActor*>& GetAllParticleActorsByIDMap() { return  SolverParticlesByID; }

	const TArray<int32>& GetSelectedParticlesIDs() const { return SelectedParticlesID; }

	void HandleVisibilitySettingsUpdated();
	void HandleColorsSettingsUpdated();

	virtual void BeginDestroy() override;

protected:

	virtual void HandlePostSelectionChange(const UTypedElementSelectionSet* ChangesSelectionSet) override;

	UPROPERTY(VisibleAnywhere, Category="Solver Data")
	int32 SolverID = INDEX_NONE;

	UPROPERTY(VisibleAnywhere, Category="Solver Data")
	FTransform SimulationTransform;

	UPROPERTY()
	TObjectPtr<UChaosVDSolverCollisionDataComponent> CollisionDataComponent;

	UPROPERTY()
	TMap<int32, AChaosVDParticleActor*> SolverParticlesByID;

	TArray<int32> SelectedParticlesID;
};
