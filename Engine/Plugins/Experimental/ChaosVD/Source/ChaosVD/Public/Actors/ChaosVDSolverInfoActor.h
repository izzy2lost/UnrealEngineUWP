// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ChaosVDSceneObjectBase.h"
#include "ChaosVDSceneSelectionObserver.h"
#include "GameFramework/Actor.h"
#include "ChaosVDSolverInfoActor.generated.h"

class AChaosVDParticleActor;
class UChaosVDSolverCollisionDataComponent;

enum class EChaosVDParticleType : uint8;

UCLASS()
class AChaosVDSolverInfoActor : public AActor, public FChaosVDSceneObjectBase, public FChaosVDSceneSelectionObserver
{
	GENERATED_BODY()

public:

	AChaosVDSolverInfoActor(const FObjectInitializer& ObjectInitializer);

	void SetSolverID(int32 InSolverID) { SolverID = InSolverID; }
	int32 GetSolverID() const { return SolverID; }

	void SetSolverName(const FString& InSolverName) { SolverName = InSolverName; }
	const FString& GetSolverName() { return SolverName; }

	void SetIsServer(bool bInIsServer) { bIsServer = bInIsServer; }
	bool GetIsServer() const { return bIsServer; }

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
	void RemoveSolverFolders(UWorld* World);

	virtual void Destroyed() override;

protected:

	virtual void HandlePostSelectionChange(const UTypedElementSelectionSet* ChangesSelectionSet) override;

	FName GetFolderPathForParticleType(EChaosVDParticleType ParticleType);

	UPROPERTY(VisibleAnywhere, Category="Solver Data")
	int32 SolverID = INDEX_NONE;

	UPROPERTY(VisibleAnywhere, Category="Solver Data")
	FTransform SimulationTransform;

	UPROPERTY(VisibleAnywhere, Category="Solver Data")
	FString SolverName;

	UPROPERTY()
	TObjectPtr<UChaosVDSolverCollisionDataComponent> CollisionDataComponent;

	UPROPERTY()
	TMap<int32, AChaosVDParticleActor*> SolverParticlesByID;

	TArray<int32> SelectedParticlesID;

	TSortedMap<EChaosVDParticleType, FName> FolderPathByParticlePath;

	TSet<FFolder> CreatedFolders;

	bool bIsServer;
};
