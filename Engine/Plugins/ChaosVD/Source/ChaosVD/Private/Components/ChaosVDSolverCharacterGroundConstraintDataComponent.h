// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "ChaosVDSolverCharacterGroundConstraintDataComponent.generated.h"

struct FChaosVDCharacterGroundConstraint;

typedef TMap<int32, TArray<TSharedPtr<FChaosVDCharacterGroundConstraint>>> FChaosVDCharacterGroundDataByParticleMap;
typedef TMap<int32, TSharedPtr<FChaosVDCharacterGroundConstraint>> FChaosVDCharacterGroundDataByConstraintIndexMap;
typedef TArray<TSharedPtr<FChaosVDCharacterGroundConstraint>> FChaosVDCharacterGroundDataArray;

enum class EChaosVDParticlePairSlot : uint8;

/** Struct used to pass data about a specific character ground constraint to other objects */
struct FChaosVDCharacterGroundConstraintSelectionHandle
{
	FChaosVDCharacterGroundConstraintSelectionHandle()
	{		
	}

	FChaosVDCharacterGroundConstraintSelectionHandle(const TWeakPtr<FChaosVDCharacterGroundConstraint>& InConstraintData)
		: ConstraintData(InConstraintData)
	{
	}

	void SetIsSelected(bool bNewSelected);
	bool IsSelected() const;

	bool IsValid();

	TWeakPtr<FChaosVDCharacterGroundConstraint> GetData() const { return ConstraintData; }

private:
	TWeakPtr<FChaosVDCharacterGroundConstraint> ConstraintData = nullptr;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FChaosVDCharacterGroundSelectionChangedDelegate, const FChaosVDCharacterGroundConstraintSelectionHandle& SelectionHandle)

UCLASS()
class CHAOSVD_API UChaosVDSolverCharacterGroundConstraintDataComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UChaosVDSolverCharacterGroundConstraintDataComponent();

	void UpdateConstraintData(const TArray<TSharedPtr<FChaosVDCharacterGroundConstraint>>& InConstraintData);
	
	const FChaosVDCharacterGroundDataArray& GetAllConstraints() const { return AllConstraints; }
	const FChaosVDCharacterGroundDataArray* GetConstraintsForParticle(int32 ParticleID) const;

	void SelectConstraint(int32 ConstraintIndex);
	void SelectConstraint(const FChaosVDCharacterGroundConstraintSelectionHandle& SelectionHandle);

	bool IsConstraintSelected(int32 ConstraintIndex) const;

	FChaosVDCharacterGroundSelectionChangedDelegate& OnSelectionChanged() { return SelectionChangeDelegate; }

	const FChaosVDCharacterGroundConstraintSelectionHandle& GetCurrentSelectionHandle() const { return CurrentConstraintSelectionHandle; }

protected:

	TSharedPtr<FChaosVDCharacterGroundConstraint> GetConstraintByIndex(int32 ConstraintIndex);
	
	void ClearData();

	FChaosVDCharacterGroundDataArray AllConstraints;
	
	FChaosVDCharacterGroundDataByParticleMap ConstraintByCharacterParticle;

	FChaosVDCharacterGroundDataByConstraintIndexMap ConstraintByConstraintIndex;

	FChaosVDCharacterGroundSelectionChangedDelegate SelectionChangeDelegate;

	FChaosVDCharacterGroundConstraintSelectionHandle CurrentConstraintSelectionHandle;
};
