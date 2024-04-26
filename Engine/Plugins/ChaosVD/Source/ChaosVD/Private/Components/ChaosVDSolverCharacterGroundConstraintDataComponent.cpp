// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/ChaosVDSolverCharacterGroundConstraintDataComponent.h"

#include "Actors/ChaosVDSolverInfoActor.h"
#include "Components/ChaosVDConstraintDataHelpers.h"
#include "DataWrappers/ChaosVDCharacterGroundConstraintDataWrappers.h"

void FChaosVDCharacterGroundConstraintSelectionHandle::SetIsSelected(bool bNewSelected)
{
	if (const TSharedPtr<FChaosVDCharacterGroundConstraint> ConstraintDataPtr = ConstraintData.Pin())
	{
		ConstraintDataPtr->bIsSelectedInEditor = bNewSelected;
	}
}

bool FChaosVDCharacterGroundConstraintSelectionHandle::IsSelected() const
{
	if (const TSharedPtr<FChaosVDCharacterGroundConstraint> ConstraintDataPtr = ConstraintData.Pin())
	{
		return ConstraintDataPtr->bIsSelectedInEditor;
	}

	return false;
}

UChaosVDSolverCharacterGroundConstraintDataComponent::UChaosVDSolverCharacterGroundConstraintDataComponent()
{
	SetCanEverAffectNavigation(false);
	bNavigationRelevant = false;
	PrimaryComponentTick.bCanEverTick = false;
}

void UChaosVDSolverCharacterGroundConstraintDataComponent::UpdateConstraintData(const TArray<TSharedPtr<FChaosVDCharacterGroundConstraint>>& InConstraintData)
{
	ClearData();

	AllConstraints = InConstraintData;

	ConstraintByCharacterParticle.Reserve(InConstraintData.Num());
	ConstraintByConstraintIndex.Reserve(InConstraintData.Num());

	for (const TSharedPtr<FChaosVDCharacterGroundConstraint>& Constraint : InConstraintData)
	{
		if (!Constraint)
		{
			continue;
		}

		ConstraintByConstraintIndex.Add(Constraint->ConstraintIndex, Constraint);
		Chaos::VisualDebugger::Utils::AddDataDataToParticleIDMap(ConstraintByCharacterParticle, Constraint, Constraint->CharacterParticleIndex);
	}
}

const FChaosVDCharacterGroundDataArray* UChaosVDSolverCharacterGroundConstraintDataComponent::GetConstraintsForParticle(int32 ParticleID) const
{
	return ConstraintByCharacterParticle.Find(ParticleID);
}

void UChaosVDSolverCharacterGroundConstraintDataComponent::SelectConstraint(int32 ConstraintIndex)
{
	CurrentConstraintSelectionHandle.SetIsSelected(false);
	CurrentConstraintSelectionHandle = FChaosVDCharacterGroundConstraintSelectionHandle(GetConstraintByIndex(ConstraintIndex));
	CurrentConstraintSelectionHandle.SetIsSelected(true);

	SelectionChangeDelegate.Broadcast(CurrentConstraintSelectionHandle);
}

void UChaosVDSolverCharacterGroundConstraintDataComponent::SelectConstraint(const FChaosVDCharacterGroundConstraintSelectionHandle& SelectionHandle)
{
	CurrentConstraintSelectionHandle.SetIsSelected(false);
	CurrentConstraintSelectionHandle = SelectionHandle;
	CurrentConstraintSelectionHandle.SetIsSelected(true);
	
	SelectionChangeDelegate.Broadcast(CurrentConstraintSelectionHandle);
}

bool UChaosVDSolverCharacterGroundConstraintDataComponent::IsConstraintSelected(int32 ConstraintIndex) const
{
	if (const TSharedPtr<FChaosVDCharacterGroundConstraint> ConstraintDataPtr = CurrentConstraintSelectionHandle.GetData().Pin())
	{
		return ConstraintDataPtr->bIsSelectedInEditor && ConstraintDataPtr->ConstraintIndex == ConstraintIndex;
	}

	return false;
}

TSharedPtr<FChaosVDCharacterGroundConstraint> UChaosVDSolverCharacterGroundConstraintDataComponent::GetConstraintByIndex(int32 ConstraintIndex)
{
	if (TSharedPtr<FChaosVDCharacterGroundConstraint>* ConstraintPtrPtr = ConstraintByConstraintIndex.Find(ConstraintIndex))
	{
		return *ConstraintPtrPtr;
	}

	return nullptr;
}

void UChaosVDSolverCharacterGroundConstraintDataComponent::ClearData()
{
	AllConstraints.Empty();
	ConstraintByCharacterParticle.Empty();
	ConstraintByConstraintIndex.Empty();
}
