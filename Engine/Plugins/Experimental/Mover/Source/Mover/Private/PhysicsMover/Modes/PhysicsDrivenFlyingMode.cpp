// Copyright Epic Games, Inc. All Rights Reserved.

#include "PhysicsMover/Modes/PhysicsDrivenFlyingMode.h"

#include "Chaos/Character/CharacterGroundConstraint.h"
#include "GameFramework/PhysicsVolume.h"
#include "MoverComponent.h"
#include "Kinematic/Settings/CommonLegacyMovementSettings.h"
#include "Math/UnitConversion.h"
#include "MoveLibrary/MovementUtils.h"
#include "PhysicsMover/PhysicsMovementUtils.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PhysicsDrivenFlyingMode)

UPhysicsDrivenFlyingMode::UPhysicsDrivenFlyingMode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UPhysicsDrivenFlyingMode::UpdateConstraintSettings(Chaos::FCharacterGroundConstraint& Constraint) const
{
	Constraint.SetRadialForceLimit(300000.0); // TEMP - Move radial force limit to shared mode data
	Constraint.SetTwistTorqueLimit(FUnitConversion::Convert(TwistTorqueLimit, EUnit::NewtonMeters, EUnit::KilogramCentimetersSquaredPerSecondSquared));
	Constraint.SetSwingTorqueLimit(FUnitConversion::Convert(SwingTorqueLimit, EUnit::NewtonMeters, EUnit::KilogramCentimetersSquaredPerSecondSquared));
	Constraint.SetTargetHeight(0.0f);
}

void UPhysicsDrivenFlyingMode::OnSimulationTick(const FSimulationTickParams& Params, FMoverTickEndData& OutputState)
{
	const FMoverTickStartData& StartState = Params.StartState;
	USceneComponent* UpdatedComponent = Params.UpdatedComponent;
	UPrimitiveComponent* UpdatedPrimitive = Params.UpdatedPrimitive;
	FProposedMove ProposedMove = Params.ProposedMove;

	const FKinematicDefaultInputs* KinematicInputs = StartState.InputCmd.InputCollection.FindDataByType<FKinematicDefaultInputs>();
	const FMoverDefaultSyncState* StartingSyncState = StartState.SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
	check(StartingSyncState);

	FMoverDefaultSyncState& OutputSyncState = OutputState.SyncState.SyncStateCollection.FindOrAddMutableDataByType<FMoverDefaultSyncState>();

	const float DeltaSeconds = Params.TimeStep.StepMs * 0.001f;
	const FVector UpDir = GetMoverComponent()->GetUpDirection();

	// Instantaneous movement changes that are executed and we exit before consuming any time
	if (ProposedMove.bHasTargetLocation && AttemptTeleport(UpdatedComponent, ProposedMove.TargetLocation, UpdatedComponent->GetComponentRotation(), *StartingSyncState, OutputState))
	{
		OutputState.MovementEndState.RemainingMs = Params.TimeStep.StepMs; 	// Give back all the time
		return;
	}

	// Don't need a floor query - just invalidate the blackboard to ensure we don't use an old result elsewhere

	if (UMoverBlackboard* SimBlackboard = GetBlackboard_Mutable())
	{
		SimBlackboard->Invalidate(KinematicBlackboard::LastFloorResult);
		SimBlackboard->Invalidate(KinematicBlackboard::LastWaterResult);
	}

	// In air steering

	FRotator TargetOrient = StartingSyncState->GetOrientation_WorldSpace();
	if (!ProposedMove.AngularVelocity.IsZero())
	{
		TargetOrient += (ProposedMove.AngularVelocity * DeltaSeconds);
	}

	FVector TargetVel = ProposedMove.LinearVelocity;
	if (const APhysicsVolume* CurPhysVolume = UpdatedComponent->GetPhysicsVolume())
	{
		// The physics simulation applies Z-only gravity acceleration via physics volumes, so we need to account for it here 
		TargetVel -= (CurPhysVolume->GetGravityZ() * FVector::UpVector * DeltaSeconds);
	}

	FVector TargetPos = StartingSyncState->GetLocation_WorldSpace() + TargetVel * DeltaSeconds;

	OutputState.MovementEndState.NextModeName = KinematicModeNames::Flying;

	OutputState.MovementEndState.RemainingMs = 0.0f;
	OutputSyncState.MoveDirectionIntent = ProposedMove.bHasDirIntent ? ProposedMove.DirectionIntent : FVector::ZeroVector;
	OutputSyncState.SetTransforms_WorldSpace(
		TargetPos,
		TargetOrient,
		TargetVel);
}