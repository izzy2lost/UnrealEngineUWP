// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "DataWrappers/ChaosVDCollisionDataWrappers.h"
#include "GameFramework/Info.h"
#include "Templates/SharedPointer.h"

#include "ChaosVDSolverCollisionDataComponent.generated.h"

class FChaosVDCollisionDataVisualizer;
class FChaosVDScene;
class FPrimitiveDrawInterface;
class FSceneView;

struct FChaosVDConstraint;

typedef TMap<int32, TArray<TSharedPtr<FChaosVDParticlePairMidPhase>>> FChaosVDMidPhaseByParticleMap;
typedef TMap<int32, TArray<FChaosVDConstraint*>> FChaosVDConstraintByParticleMap;

enum class EChaosVDGetCollisionDataOptions
{
	Owner,
	Secondary,
	Any
};

UCLASS()
class UChaosVDSolverCollisionDataComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UChaosVDSolverCollisionDataComponent();

	void UpdateCollisionData(const TArray<TSharedPtr<FChaosVDParticlePairMidPhase>>& InMidPhaseData);

	const TArray<TSharedPtr<FChaosVDParticlePairMidPhase>>& GetMidPhases() const { return AllMidPhases; }
	const TArray<TSharedPtr<FChaosVDParticlePairMidPhase>>* GetMidPhasesForParticle(int32 ParticleID, EChaosVDGetCollisionDataOptions Options) const;
	const TArray<FChaosVDConstraint*>* GetConstraintsForParticle(int32 ParticleID, EChaosVDGetCollisionDataOptions Options) const;

	void DrawVisualization(const FSceneView* View, FPrimitiveDrawInterface* PDI);

protected:

	void ClearCollisionData();

	template<typename MapType, typename CollisionDataType>
	void AddCollisionDataToParticleIDMap(MapType& MapToUpdate, const CollisionDataType& MidPhaseData, int32 ParticleID);

	template<typename MapType, typename CollisionDataType>
	const TArray<CollisionDataType>* GetCollisionDataFromMap(const MapType& Map0ToQuery, const MapType& Map1ToQuery, int32 ParticleID, EChaosVDGetCollisionDataOptions Options) const;

	TArray<TSharedPtr<FChaosVDParticlePairMidPhase>> AllMidPhases;
	FChaosVDMidPhaseByParticleMap MidPhasesByParticleID0;
	FChaosVDMidPhaseByParticleMap MidPhasesByParticleID1;

	FChaosVDConstraintByParticleMap ConstraintsByParticleID0;
	FChaosVDConstraintByParticleMap ConstraintsByParticleID1;

};

template <typename MapType, typename CollisionDataType>
void UChaosVDSolverCollisionDataComponent::AddCollisionDataToParticleIDMap(MapType& MapToUpdate, const CollisionDataType& MidPhaseData, int32 ParticleID)
{
	if (TArray<CollisionDataType>* ParticleCollisionData = MapToUpdate.Find(ParticleID))
	{
		ParticleCollisionData->Add(MidPhaseData);
	}
	else
	{
		MapToUpdate.Add(ParticleID, { MidPhaseData });
	}
}

template <typename MapType, typename CollisionDataType>
const TArray<CollisionDataType>* UChaosVDSolverCollisionDataComponent::GetCollisionDataFromMap(const MapType& Map0ToQuery, const MapType& Map1ToQuery, int32 ParticleID, EChaosVDGetCollisionDataOptions Options) const
{
	switch (Options)
	{
	case EChaosVDGetCollisionDataOptions::Owner:
		{
			return Map0ToQuery.Find(ParticleID);
			break;
		}
	case EChaosVDGetCollisionDataOptions::Secondary:
		{
			return Map1ToQuery.Find(ParticleID);
			break;
		}
	case EChaosVDGetCollisionDataOptions::Any:
		{
			const TArray<CollisionDataType>* FoundMidPhasesContainer = Map0ToQuery.Find(ParticleID);
			return FoundMidPhasesContainer ? FoundMidPhasesContainer : Map1ToQuery.Find(ParticleID);
			break;
		}
	}
	return nullptr;
}
