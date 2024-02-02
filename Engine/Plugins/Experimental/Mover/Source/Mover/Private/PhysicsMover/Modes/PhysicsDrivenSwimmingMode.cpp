// Copyright Epic Games, Inc. All Rights Reserved.

#include "PhysicsMover/Modes/PhysicsDrivenSwimmingMode.h"

#include "Chaos/Character/CharacterGroundConstraint.h"
#include "Math/UnitConversion.h"
#include "MoveLibrary/FloorQueryUtils.h"
#include "MoveLibrary/WaterMovementUtils.h"
#include "Kinematic/LayeredMoves/BasicLayeredMoves.h"
#include "MoverComponent.h"
#include "Kinematic/Settings/CommonLegacyMovementSettings.h"
#include "GameFramework/PhysicsVolume.h"
#include "PhysicsMover/PhysicsMovementUtils.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PhysicsDrivenSwimmingMode)

UPhysicsDrivenSwimmingMode::UPhysicsDrivenSwimmingMode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UPhysicsDrivenSwimmingMode::UpdateConstraintSettings(Chaos::FCharacterGroundConstraint& Constraint) const
{
	Constraint.SetSwingTorqueLimit(FUnitConversion::Convert(3000.0f, EUnit::NewtonMeters, EUnit::KilogramCentimetersSquaredPerSecondSquared));
	Constraint.SetRadialForceLimit(0.0f);
	Constraint.SetFrictionForceLimit(0.0f);
	Constraint.SetTwistTorqueLimit(0.0f);
}

void UPhysicsDrivenSwimmingMode::OnSimulationTick(const FSimulationTickParams& Params, FMoverTickEndData& OutputState)
{
	const UMoverComponent* MoverComp = GetMoverComponent();

	const FMoverTickStartData& StartState = Params.StartState;
	USceneComponent* UpdatedComponent = Params.UpdatedComponent;
	UPrimitiveComponent* UpdatedPrimitive = Params.UpdatedPrimitive;
	FProposedMove ProposedMove = Params.ProposedMove;
	
	const FVector UpDir = GetMoverComponent()->GetUpDirection();
	
	const FKinematicDefaultInputs* KinematicInputs = StartState.InputCmd.InputCollection.FindDataByType<FKinematicDefaultInputs>();

	const FMoverDefaultSyncState* StartingSyncState = StartState.SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
	check(StartingSyncState);

	FMoverDefaultSyncState& OutputSyncState = OutputState.SyncState.SyncStateCollection.FindOrAddMutableDataByType<FMoverDefaultSyncState>();

	const float DeltaSeconds = Params.TimeStep.StepMs * 0.001f;

	if ((ProposedMove.bHasTargetLocation && AttemptTeleport(UpdatedComponent, ProposedMove.TargetLocation, UpdatedComponent->GetComponentRotation(), StartingSyncState->GetVelocity_WorldSpace(), OutputState)) ||	// Teleport
		(KinematicInputs->bIsJumpJustPressed && AttemptJump(SurfaceSwimmingWaterControlSettings.JumpMultiplier*CommonLegacySettings->JumpUpwardsSpeed, OutputState)))
	{
		OutputState.MovementEndState.RemainingMs = Params.TimeStep.StepMs;
		return;
	}
	
	UMoverBlackboard* SimBlackboard = GetBlackboard_Mutable();
	if (!SimBlackboard)
	{
		OutputSyncState = *StartingSyncState;
		return;
	}
	SimBlackboard->Invalidate(KinematicBlackboard::LastFloorResult);
	SimBlackboard->Invalidate(KinematicBlackboard::LastWaterResult);
	
	OutputSyncState.MoveDirectionIntent = ProposedMove.bHasDirIntent ? ProposedMove.DirectionIntent : FVector::ZeroVector;

	// Floor query
	FFloorCheckResult FloorResult;
	FWaterCheckResult WaterResult;
	
	float PawnHalfHeight;
	float PawnRadius;
	UpdatedPrimitive->CalcBoundingCylinder(PawnRadius, PawnHalfHeight);
	
	UPhysicsMovementUtils::FindFloor(StartingSyncState->GetLocation_WorldSpace(), StartingSyncState->GetVelocity_WorldSpace() * DeltaSeconds,
		UpdatedPrimitive, UpDir, PawnRadius, TargetHeight, CommonLegacySettings->MaxStepHeight,
		CommonLegacySettings->MaxWalkSlopeCosine, FloorResult, WaterResult);
	
	SimBlackboard->Set(KinematicBlackboard::LastFloorResult, FloorResult);
	SimBlackboard->Set(KinematicBlackboard::LastWaterResult, WaterResult);
	
	if (WaterResult.IsSwimmableVolume())
	{
		const bool bIsWithinReach = FloorResult.FloorDist <= TargetHeight;
		const bool bWalkTrigger = WaterResult.WaterSplineData.ImmersionDepth < CommonLegacySettings->SwimmingStopImmersionDepth;
		const bool bFallTrigger = FMath::Clamp((WaterResult.WaterSplineData.ImmersionDepth + TargetHeight) / (2 * TargetHeight), -2.f, 2.f) < -1.f;
	
		FRotator TargetOrient = StartingSyncState->GetOrientation_WorldSpace();
		if (!ProposedMove.AngularVelocity.IsZero())
		{
			TargetOrient += (ProposedMove.AngularVelocity * DeltaSeconds);
		}

		FVector TargetVel = ProposedMove.LinearVelocity;		
		if (const APhysicsVolume* CurPhysVolume = UpdatedComponent->GetPhysicsVolume())
		{
			// Discount G Forces as Buoyancy accounts for it
			TargetVel -= (CurPhysVolume->GetGravityZ() * FVector::UpVector * DeltaSeconds);
		}
		
		FVector TargetPos = StartingSyncState->GetLocation_WorldSpace();
		TargetPos += TargetVel * DeltaSeconds;
	
		OutputSyncState.SetTransforms_WorldSpace(
			TargetPos,
			TargetOrient,
			TargetVel);
	
		if (bWalkTrigger && bIsWithinReach)
		{
			OutputState.MovementEndState.NextModeName = KinematicModeNames::Walking;
		}
		else if (bFallTrigger)
		{
			OutputState.MovementEndState.NextModeName = KinematicModeNames::Falling;
		}
		else
		{
			OutputState.MovementEndState.NextModeName = KinematicModeNames::Swimming;
		}
	}
	else
	{
		OutputState.MovementEndState.NextModeName = KinematicModeNames::Falling;
	}
	
	OutputState.MovementEndState.RemainingMs = 0.0f;
}

bool UPhysicsDrivenSwimmingMode::AttemptJump(float UpwardsSpeed, FMoverTickEndData& OutputState)
{
	FMoverDefaultSyncState& OutputSyncState = OutputState.SyncState.SyncStateCollection.FindOrAddMutableDataByType<FMoverDefaultSyncState>();

	// TODO: This should check if a jump is even allowed
 	TSharedPtr<FLayeredMove_JumpImpulse> JumpMove = MakeShared<FLayeredMove_JumpImpulse>();
	JumpMove->UpwardsSpeed = UpwardsSpeed;
	OutputSyncState.LayeredMoves.QueueLayeredMove(JumpMove);
	OutputState.MovementEndState.NextModeName = KinematicModeNames::Falling;
	return true;
}

bool UPhysicsDrivenSwimmingMode::AttemptTeleport(USceneComponent* UpdatedComponent, const FVector& TeleportPos, const FRotator& TeleportRot, const FVector& PriorVelocity, FMoverTickEndData& Output)
{
	FMoverDefaultSyncState& OutputSyncState = Output.SyncState.SyncStateCollection.FindOrAddMutableDataByType<FMoverDefaultSyncState>();

	OutputSyncState.SetTransforms_WorldSpace(TeleportPos,
		TeleportRot,
		PriorVelocity,
		nullptr); // no movement base

	// TODO: instead of invalidating it, consider checking for a floor. Possibly a dynamic base?
	GetBlackboard_Mutable()->Invalidate(KinematicBlackboard::LastFloorResult);
	GetBlackboard_Mutable()->Invalidate(KinematicBlackboard::LastMovementBase);

	return true;
}