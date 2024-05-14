// Copyright Epic Games, Inc. All Rights Reserved.

#include "DefaultMovementSet/LayeredMoves/AnimRootMotionLayeredMove.h"
#include "MoverComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "MoveLibrary/MovementUtils.h"
#include "MoverTypes.h"
#include "MoverLog.h"
#include "MotionWarpingComponent.h"


FLayeredMove_AnimRootMotion::FLayeredMove_AnimRootMotion()
{
	DurationMs = 0.f;
	MixMode = EMoveMixMode::OverrideAll;

	Montage = nullptr;
	StartingMontagePosition = 0.f;
	PlayRate = 1.f;
}

bool FLayeredMove_AnimRootMotion::GenerateMove(const FMoverTickStartData& SimState, const FMoverTimeStep& TimeStep, const UMoverComponent* MoverComp, UMoverBlackboard* SimBlackboard, FProposedMove& OutProposedMove)
{
	const float DeltaSeconds = TimeStep.StepMs / 1000.f;

	const AActor* MoverActor = MoverComp->GetOwner();

	const FMoverDefaultSyncState* SyncState = SimState.SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();

	// First pass simply samples based on the duration. For long animations, this has the potential to diverge.
	// Future improvements could include:
	//     - speeding up or slowing down slightly to match the associated montage instance
	//     - detecting if the montage instance is interrupted and attempting to interrupt and scheduling this move to end at the same sim time
	
	// Note that Montage 'position' equates to seconds when PlayRate is 1
	const float SecondsSinceMontageStarted = (TimeStep.BaseSimTimeMs - StartSimTimeMs) / 1000.f;
	const float ScaledSecondsSinceMontageStarted = SecondsSinceMontageStarted * PlayRate;

	const float ExtractionStartPosition = StartingMontagePosition + ScaledSecondsSinceMontageStarted;
	const float ExtractionEndPosition   = ExtractionStartPosition + (DeltaSeconds * PlayRate);

	// Read the local transform directly from the montage
	const FTransform LocalRootMotion = UMotionWarpingUtilities::ExtractRootMotionFromAnimation(Montage, ExtractionStartPosition, ExtractionEndPosition);

	FMotionWarpingUpdateContext WarpingContext;
	WarpingContext.Animation = Montage;
	WarpingContext.CurrentPosition = ExtractionEndPosition;
	WarpingContext.PreviousPosition = ExtractionStartPosition;
	WarpingContext.PlayRate = PlayRate;
	WarpingContext.Weight = 1.f;

	// Note that we're forcing the use of the sync state's actor transform data. This is necessary when the movement simulation 
	// is running ahead of the actor's visual representation and may be rotated differently, such as in an async physics sim.
	const FTransform SimActorTransform = FTransform(SyncState->GetOrientation_WorldSpace().Quaternion(), SyncState->GetLocation_WorldSpace());
	const FTransform WorldSpaceRootMotion = MoverComp->ConvertLocalRootMotionToWorld(LocalRootMotion, DeltaSeconds, &SimActorTransform, &WarpingContext);
	
	OutProposedMove = FProposedMove();
	OutProposedMove.MixMode = MixMode;

	// Convert the transform into linear and angular velocities
	OutProposedMove.LinearVelocity    = WorldSpaceRootMotion.GetTranslation() / DeltaSeconds;
	OutProposedMove.AngularVelocity   = WorldSpaceRootMotion.GetRotation().Rotator() * (1.f / DeltaSeconds);

	return true;
}

FLayeredMoveBase* FLayeredMove_AnimRootMotion::Clone() const
{
	FLayeredMove_AnimRootMotion* CopyPtr = new FLayeredMove_AnimRootMotion(*this);
	return CopyPtr;
}

void FLayeredMove_AnimRootMotion::NetSerialize(FArchive& Ar)
{
	Super::NetSerialize(Ar);

	Ar << Montage;
	Ar << StartingMontagePosition;
	Ar << PlayRate;
}

UScriptStruct* FLayeredMove_AnimRootMotion::GetScriptStruct() const
{
	return FLayeredMove_AnimRootMotion::StaticStruct();
}

FString FLayeredMove_AnimRootMotion::ToSimpleString() const
{
	return FString::Printf(TEXT("AnimRootMotion"));
}

void FLayeredMove_AnimRootMotion::AddReferencedObjects(class FReferenceCollector& Collector)
{
	Super::AddReferencedObjects(Collector);
}
