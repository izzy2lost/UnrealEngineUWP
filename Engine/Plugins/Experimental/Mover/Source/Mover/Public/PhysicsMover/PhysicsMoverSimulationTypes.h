// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Chaos/GeometryParticlesfwd.h"
#include "MoverSimulationTypes.h"
#include "MoveLibrary/FloorQueryUtils.h"
#include "UObject/Interface.h"
#include "UObject/WeakInterfacePtr.h"
#include "PhysicsMoverSimulationTypes.generated.h"

//////////////////////////////////////////////////////////////////////////
// Debug

struct FPhysicsDrivenMotionDebugParams
{
	float TeleportThreshold = 100.0f;
	float MinStepUpDistance = 5.0f;
	bool EnableMultithreading = true;
	bool DebugDrawGroundQueries = false;
};

#ifndef PHYSICSDRIVENMOTION_DEBUG_DRAW
#define PHYSICSDRIVENMOTION_DEBUG_DRAW (!(UE_BUILD_SHIPPING || UE_BUILD_TEST) || WITH_EDITOR)
#endif

//////////////////////////////////////////////////////////////////////////
// Async update

struct FPhysicsMoverSimulationTickParams
{
	float SimTimeSeconds = 0.0f;
	float DeltaTimeSeconds = 0.0f;
};

struct FPhysicsMoverSimulationInput
{
	FMoverInputCmdContext InputCmd;
	FMoverSyncState SyncState;
	FMoverAuxStateContext AuxState;
};

struct FPhysicsMoverSimulationOutput
{
	bool IsValid() const { return bIsValid; }

	FMoverSyncState SyncState;
	FMoverAuxStateContext AuxState;
	FFloorCheckResult FloorResult;
	bool bIsValid = false;
};

namespace Chaos { class FCharacterGroundConstraintHandle; }

struct FPhysicsMoverAsyncInput
{
	bool IsValid() const
	{
		return MoverSimulation.IsValid() && MoverIdx.IsValid();
	}

	// Input is modified during ProcessInputs_Internal
	mutable FMoverInputCmdContext InputCmd;
	mutable FMoverSyncState SyncState;
	mutable FMoverAuxStateContext AuxState;

	TWeakObjectPtr<class UMoverNetworkPhysicsLiaisonComponent> MoverSimulation;
	Chaos::FUniqueIdx MoverIdx;
};

struct FPhysicsMoverAsyncOutput
{
	FMoverSyncState SyncState;
	FMoverAuxStateContext AuxState;
	FFloorCheckResult FloorResult;
	bool bIsValid = false;
};

//////////////////////////////////////////////////////////////////////////
// Movement modes

/**
 * PhysicsDrivenMotionMode: Interface for movement modes that are for physics driven motion
 * A physics driven motion mode needs to update the character ground constraint with the
 * parameters associated with that mode
 */
UINTERFACE(MinimalAPI)
class UPhysicsCharacterMovementModeInterface : public UInterface
{
	GENERATED_BODY()
};

namespace Chaos { class FCharacterGroundConstraint; }

class IPhysicsCharacterMovementModeInterface
{
	GENERATED_BODY()

public:
	virtual void UpdateConstraintSettings(Chaos::FCharacterGroundConstraint& Constraint) const = 0;
};