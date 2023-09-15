// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/ChaosVDSolverCollisionDataComponent.h"

#include "DataWrappers/ChaosVDCollisionDataWrappers.h"

UChaosVDSolverCollisionDataComponent::UChaosVDSolverCollisionDataComponent()
{
	SetCanEverAffectNavigation(false);
	bNavigationRelevant = false;
}

void UChaosVDSolverCollisionDataComponent::UpdateCollisionData(const TArray<TSharedPtr<FChaosVDParticlePairMidPhase>>& InMidPhaseData)
{
	ClearCollisionData();

	AllMidPhases.Reserve(InMidPhaseData.Num());
	AllMidPhases = InMidPhaseData;

	MidPhasesByParticleID0.Reserve(InMidPhaseData.Num());
	MidPhasesByParticleID1.Reserve(InMidPhaseData.Num());

	for (const TSharedPtr<FChaosVDParticlePairMidPhase>& ParticleIDMidPhase : InMidPhaseData)
	{
		AddCollisionDataToParticleIDMap(MidPhasesByParticleID0, ParticleIDMidPhase, ParticleIDMidPhase->Particle0Idx);
		AddCollisionDataToParticleIDMap(MidPhasesByParticleID1, ParticleIDMidPhase, ParticleIDMidPhase->Particle1Idx);

		ConstraintsByParticleID0.Reserve(ParticleIDMidPhase->Constraints.Num());
		ConstraintsByParticleID1.Reserve(ParticleIDMidPhase->Constraints.Num());
		for (FChaosVDConstraint& Constraint : ParticleIDMidPhase->Constraints)
		{
			AddCollisionDataToParticleIDMap<FChaosVDConstraintByParticleMap, FChaosVDConstraint*>(ConstraintsByParticleID0, &Constraint, Constraint.Particle0Index);
			AddCollisionDataToParticleIDMap<FChaosVDConstraintByParticleMap, FChaosVDConstraint*>(ConstraintsByParticleID1, &Constraint, Constraint.Particle1Index);
		}
	}
}

const TArray<TSharedPtr<FChaosVDParticlePairMidPhase>>* UChaosVDSolverCollisionDataComponent::GetMidPhasesForParticle(int32 ParticleID, EChaosVDCollisionParticlePairSlot Options) const
{
	return GetCollisionDataFromMap<FChaosVDMidPhaseByParticleMap, TSharedPtr<FChaosVDParticlePairMidPhase>>(MidPhasesByParticleID0, MidPhasesByParticleID1, ParticleID, Options);
}

const TArray<FChaosVDConstraint*>* UChaosVDSolverCollisionDataComponent::GetConstraintsForParticle(int32 ParticleID, EChaosVDCollisionParticlePairSlot Options) const
{
	return GetCollisionDataFromMap<FChaosVDConstraintByParticleMap, FChaosVDConstraint*>(ConstraintsByParticleID0, ConstraintsByParticleID1, ParticleID, Options);
}

void UChaosVDSolverCollisionDataComponent::DrawVisualization(const FSceneView* View, FPrimitiveDrawInterface* PDI)
{
}

void UChaosVDSolverCollisionDataComponent::ClearCollisionData()
{
	AllMidPhases.Reset();
	MidPhasesByParticleID0.Reset();
	MidPhasesByParticleID1.Reset();
	ConstraintsByParticleID0.Reset();
	ConstraintsByParticleID1.Reset();
}