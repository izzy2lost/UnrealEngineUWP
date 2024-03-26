// Copyright Epic Games, Inc. All Rights Reserved.

#include "PhysicsMover/Modes/PhysicsDrivenWalkingMode.h"

#include "Chaos/Character/CharacterGroundConstraint.h"
#include "Chaos/PhysicsObject.h"
#include "Chaos/PhysicsObjectInternalInterface.h"
#include "GameFramework/PhysicsVolume.h"
#include "DefaultMovementSet/LayeredMoves/BasicLayeredMoves.h"
#include "MoverComponent.h"
#include "DefaultMovementSet/Settings/CommonLegacyMovementSettings.h"
#include "Math/UnitConversion.h"
#include "MoveLibrary/WaterMovementUtils.h"
#include "PhysicsMover/PhysicsMovementUtils.h"
#include "PhysicsMover/PhysicsMoverSimulationTypes.h"
#if WITH_EDITOR
#include "Backends/MoverNetworkPhysicsLiaison.h"
#include "Internationalization/Text.h"
#include "Misc/DataValidation.h"
#endif // WITH_EDITOR

#include UE_INLINE_GENERATED_CPP_BY_NAME(PhysicsDrivenWalkingMode)

extern FPhysicsDrivenMotionDebugParams GPhysicsDrivenMotionDebugParams;

#define LOCTEXT_NAMESPACE "PhysicsDrivenWalkingMode"

UPhysicsDrivenWalkingMode::UPhysicsDrivenWalkingMode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UPhysicsDrivenWalkingMode::UpdateConstraintSettings(Chaos::FCharacterGroundConstraint& Constraint) const
{
	Constraint.SetRadialForceLimit(FUnitConversion::Convert(RadialForceLimit, EUnit::Newtons, EUnit::KilogramCentimetersPerSecondSquared));
	Constraint.SetFrictionForceLimit(FUnitConversion::Convert(FrictionForceLimit, EUnit::Newtons, EUnit::KilogramCentimetersPerSecondSquared));
	Constraint.SetTwistTorqueLimit(FUnitConversion::Convert(TwistTorqueLimit, EUnit::NewtonMeters, EUnit::KilogramCentimetersSquaredPerSecondSquared));
	Constraint.SetSwingTorqueLimit(FUnitConversion::Convert(SwingTorqueLimit, EUnit::NewtonMeters, EUnit::KilogramCentimetersSquaredPerSecondSquared));
	Constraint.SetTargetHeight(TargetHeight);
}

#if WITH_EDITOR
EDataValidationResult UPhysicsDrivenWalkingMode::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	const UClass* BackendClass = GetMoverComponent()->BackendClass;
	if (BackendClass && !BackendClass->IsChildOf<UMoverNetworkPhysicsLiaisonComponent>())
	{
		Context.AddError(LOCTEXT("PhysicsMovementModeHasValidPhysicsLiaison", "Physics movement modes need to have a backend class that supports physics (UMoverNetworkPhysicsLiaisonComponent)."));
		Result = EDataValidationResult::Invalid;
	}
		
	return Result;
}
#endif // WITH_EDITOR

void UPhysicsDrivenWalkingMode::OnSimulationTick(const FSimulationTickParams& Params, FMoverTickEndData& OutputState)
{
	const UMoverComponent* MoverComp = GetMoverComponent();
	const FMoverTickStartData& StartState = Params.StartState;
	USceneComponent* UpdatedComponent = Params.UpdatedComponent;
	UPrimitiveComponent* UpdatedPrimitive = Params.UpdatedPrimitive;
	FProposedMove ProposedMove = Params.ProposedMove;

	const FVector UpDir = GetMoverComponent()->GetUpDirection();

	const FMoverDefaultSyncState* StartingSyncState = StartState.SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
	check(StartingSyncState);

	FMoverDefaultSyncState& OutputSyncState = OutputState.SyncState.SyncStateCollection.FindOrAddMutableDataByType<FMoverDefaultSyncState>();

	const float DeltaSeconds = Params.TimeStep.StepMs * 0.001f;

	// Instantaneous movement changes that are executed and we exit before consuming any time
	if ((ProposedMove.bHasTargetLocation && AttemptTeleport(UpdatedComponent, ProposedMove.TargetLocation, UpdatedComponent->GetComponentRotation(), StartingSyncState->GetVelocity_WorldSpace(), OutputState)))
	{
		OutputState.MovementEndState.RemainingMs = Params.TimeStep.StepMs; 	// Give back all the time
		return;
	}

	UMoverBlackboard* SimBlackboard = GetBlackboard_Mutable();
	if (!SimBlackboard)
	{
		OutputSyncState = *StartingSyncState;
		return;
	}

	// Store the previous ground normal that was used to compute the proposed move
	FFloorCheckResult PrevFloorResult;
	FVector PrevGroundNormal = UpDir;
	if (SimBlackboard->TryGet(CommonBlackboard::LastFloorResult, PrevFloorResult))
	{
		PrevGroundNormal = PrevFloorResult.HitResult.ImpactNormal;
	}

	// Floor query
	float PawnHalfHeight;
	float PawnRadius;
	UpdatedPrimitive->CalcBoundingCylinder(PawnRadius, PawnHalfHeight);

	FFloorCheckResult FloorResult;
	FWaterCheckResult WaterResult;
	UPhysicsMovementUtils::FindFloor(StartingSyncState->GetLocation_WorldSpace(), StartingSyncState->GetVelocity_WorldSpace() * DeltaSeconds,
		UpdatedPrimitive, UpDir, PawnRadius, TargetHeight, CommonLegacySettings->MaxStepHeight,
		CommonLegacySettings->MaxWalkSlopeCosine, FloorResult, WaterResult);

	SimBlackboard->Set(CommonBlackboard::LastFloorResult, FloorResult);
	SimBlackboard->Set(CommonBlackboard::LastWaterResult, WaterResult);

	const bool bStartSwimming = WaterResult.WaterSplineData.ImmersionDepth > CommonLegacySettings->SwimmingStartImmersionDepth;
	
	if (WaterResult.IsSwimmableVolume() && bStartSwimming)
	{
		SwitchToState(DefaultModeNames::Swimming, Params, OutputState);
	}
	else if (FloorResult.IsWalkableFloor())
	{
		const FVector StartGroundVelocity = UPhysicsMovementUtils::ComputeGroundVelocityFromHitResult(StartingSyncState->GetLocation_WorldSpace(), FloorResult.HitResult, DeltaSeconds);
		
		FVector TargetVelocity = StartingSyncState->GetVelocity_WorldSpace();
		FVector TargetPosition = StartingSyncState->GetLocation_WorldSpace();
		if (FloorResult.bWalkableFloor)
		{
			const FVector ProposedMovePlaneVelocity = ProposedMove.LinearVelocity - ProposedMove.LinearVelocity.ProjectOnToNormal(PrevGroundNormal);
			const FVector StartingMovePlaneVelocity = StartingSyncState->GetVelocity_WorldSpace() - StartingSyncState->GetVelocity_WorldSpace().ProjectOnToNormal(PrevGroundNormal);

			// If there is velocity intent in the normal direction then use the velocity from the proposed move. Otherwise
			// retain the previous vertical velocity
			FVector ProposedNormalVelocity = ProposedMove.LinearVelocity - ProposedMovePlaneVelocity;
			if (ProposedNormalVelocity.SizeSquared() <= UE_SMALL_NUMBER)
			{
				ProposedNormalVelocity = StartingSyncState->GetVelocity_WorldSpace() - StartingMovePlaneVelocity;
			}

			TargetVelocity = FMath::Lerp(StartingMovePlaneVelocity, ProposedMovePlaneVelocity, FractionalVelocityToTarget) + ProposedNormalVelocity;
			TargetPosition += ProposedMovePlaneVelocity * DeltaSeconds;
		}

		// Check if the proposed velocity would lift off the movement surface.
		bool bIsLiftingOffSurface = false;

		float CharacterGravity = 0.0f;
		if (const APhysicsVolume* PhysVolume = UpdatedComponent->GetPhysicsVolume())
		{
			CharacterGravity = PhysVolume->GetGravityZ();
		}
		const FVector ProjectedVelocity = TargetVelocity + CharacterGravity * FVector::UpVector * DeltaSeconds;
		const FVector ProjectedGroundVelocity = UPhysicsMovementUtils::ComputeIntegratedGroundVelocityFromHitResult(StartingSyncState->GetLocation_WorldSpace(), FloorResult.HitResult, DeltaSeconds);

		const float ProjectedRelativeVerticalVelocity = FloorResult.HitResult.ImpactNormal.Dot(ProjectedVelocity - ProjectedGroundVelocity);
		const float VerticalVelocityLimit = 2.0f / DeltaSeconds;
		if (ProjectedRelativeVerticalVelocity > VerticalVelocityLimit)
		{
			bIsLiftingOffSurface = true;
		}

		// Determine if the character is stepping up or stepping down.
		// If stepping up make sure that the step height is less than the max step height
		// and the new surface has CanCharacterStepUpOn set to true.
		// If stepping down make sure the step height is less than the max step height.
		const float StartHeightAboveGround = FloorResult.FloorDist - TargetHeight;
		const float EndHeightAboutGround = StartHeightAboveGround + UpDir.Dot(ProjectedVelocity - ProjectedGroundVelocity) * DeltaSeconds ;
		const bool bIsSteppingDown = StartHeightAboveGround > GPhysicsDrivenMotionDebugParams.MinStepUpDistance;
		const bool bIsWithinReach = EndHeightAboutGround <= CommonLegacySettings->MaxStepHeight;

		// If the character is unsupported allow some grace period before falling
		bool bIsSupported = bIsWithinReach && !bIsLiftingOffSurface;
		float TimeSinceSupported = MaxUnsupportedTimeBeforeFalling;
		SimBlackboard->TryGet(CommonBlackboard::TimeSinceSupported, TimeSinceSupported);
		if (bIsSupported)
		{
			SimBlackboard->Set(CommonBlackboard::TimeSinceSupported, 0.0f);
		}
		else
		{
			TimeSinceSupported += DeltaSeconds;
			SimBlackboard->Set(CommonBlackboard::TimeSinceSupported, TimeSinceSupported);
			bIsSupported = TimeSinceSupported < MaxUnsupportedTimeBeforeFalling;
		}

		// Apply vertical velocity to target if stepping down
		const bool bNeedsVerticalVelocityToTarget = bIsSupported && bIsSteppingDown && (EndHeightAboutGround > 0.0f) && !bIsLiftingOffSurface;
		if (bNeedsVerticalVelocityToTarget)
		{
			TargetVelocity -= FractionalDownwardVelocityToTarget * (EndHeightAboutGround / DeltaSeconds) * UpDir;
		}

		// Target orientation
		// This is always applied regardless of whether the character is supported
		FRotator TargetOrientation = StartingSyncState->GetOrientation_WorldSpace();
		if (!ProposedMove.AngularVelocity.IsZero())
		{
			TargetOrientation += (ProposedMove.AngularVelocity * DeltaSeconds);
		}

		if (bIsSupported)
		{
			OutputState.MovementEndState.NextModeName = DefaultModeNames::Walking;
			OutputState.MovementEndState.RemainingMs = 0.0f;

			OutputSyncState.MoveDirectionIntent = ProposedMove.bHasDirIntent ? ProposedMove.DirectionIntent : FVector::ZeroVector;
			OutputSyncState.SetTransforms_WorldSpace(
				TargetPosition,
				TargetOrientation,
				TargetVelocity,
				nullptr);
		}
		else
		{
			// Blocking hit but not supported
			SwitchToState(DefaultModeNames::Falling, Params, OutputState);
		}
	}
	else
	{
		// No water or floor not found
		SwitchToState(DefaultModeNames::Falling, Params, OutputState);
	}
}

bool UPhysicsDrivenWalkingMode::AttemptTeleport(USceneComponent* UpdatedComponent, const FVector& TeleportPos, const FRotator& TeleportRot, const FVector& PriorVelocity, FMoverTickEndData& Output)
{
	FMoverDefaultSyncState& OutputSyncState = Output.SyncState.SyncStateCollection.FindOrAddMutableDataByType<FMoverDefaultSyncState>();

	OutputSyncState.SetTransforms_WorldSpace(TeleportPos,
		TeleportRot,
		PriorVelocity,
		nullptr); // no movement base

	// TODO: instead of invalidating it, consider checking for a floor. Possibly a dynamic base?
	if (UMoverBlackboard* SimBlackboard = GetBlackboard_Mutable())
	{
		SimBlackboard->Invalidate(CommonBlackboard::LastFloorResult);
		SimBlackboard->Invalidate(CommonBlackboard::LastFoundDynamicMovementBase);
	}

	return true;
}

void UPhysicsDrivenWalkingMode::SwitchToState(const FName& StateName, const FSimulationTickParams& Params, FMoverTickEndData& OutputState)
{
	OutputState.MovementEndState.RemainingMs = Params.TimeStep.StepMs;
	OutputState.MovementEndState.NextModeName = StateName;

	const FMoverDefaultSyncState* StartingSyncState = Params.StartState.SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
	FMoverDefaultSyncState& OutputSyncState = OutputState.SyncState.SyncStateCollection.FindOrAddMutableDataByType<FMoverDefaultSyncState>();
	OutputSyncState.SetTransforms_WorldSpace(
		StartingSyncState->GetLocation_WorldSpace(),
		StartingSyncState->GetOrientation_WorldSpace(),
		StartingSyncState->GetVelocity_WorldSpace(),
		nullptr);
}

#undef LOCTEXT_NAMESPACE