// Copyright Epic Games, Inc. All Rights Reserved.

#include "MoveLibrary/BasedMovementUtils.h"
#include "MoveLibrary/MovementUtils.h"
#include "MoveLibrary/FloorQueryUtils.h"
#include "Components/PrimitiveComponent.h"
#include "MoverLog.h"
#include "Kismet/KismetMathLibrary.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BasedMovementUtils)

void FRelativeBaseInfo::Clear()
{
	MovementBase = nullptr;
	BoneName = NAME_None;
	Location = FVector::ZeroVector;
	Rotation = FQuat::Identity;
}

bool FRelativeBaseInfo::HasRelativeInfo() const
{
	return MovementBase != nullptr;
}

bool FRelativeBaseInfo::UsesSameBase(const FRelativeBaseInfo& Other) const
{
	return UsesSameBase(Other.MovementBase, Other.BoneName);
}

bool FRelativeBaseInfo::UsesSameBase(const UPrimitiveComponent* OtherComp, FName OtherBoneName) const
{
	return HasRelativeInfo()
		&& (MovementBase == OtherComp)
		&& (BoneName == OtherBoneName);
}

void FRelativeBaseInfo::SetFromFloorResult(const FFloorCheckResult& FloorTestResult)
{
	bool bDidSucceed = false;

	if (FloorTestResult.bWalkableFloor)
	{
		MovementBase = FloorTestResult.HitResult.GetComponent();

		if (MovementBase)
		{
			BoneName = FloorTestResult.HitResult.BoneName;
			bDidSucceed  = UBasedMovementUtils::GetMovementBaseTransform(MovementBase, BoneName, /*out*/Location, /*out*/Rotation);
			bDidSucceed &= UBasedMovementUtils::TransformWorldLocationToBased(MovementBase, BoneName, FloorTestResult.HitResult.ImpactPoint, /*out*/ContactLocalPosition);
		}
	}

	if (!bDidSucceed)
	{
		Clear();
	}
}

void FRelativeBaseInfo::SetFromComponent(UPrimitiveComponent* InRelativeComp, FName InBoneName)
{
	bool bDidSucceed = false;

	MovementBase = InRelativeComp;

	if (MovementBase)
	{
		BoneName = InBoneName;
		bDidSucceed = UBasedMovementUtils::GetMovementBaseTransform(MovementBase, BoneName, /*out*/Location, /*out*/Rotation);
	}

	if (!bDidSucceed)
	{
		Clear();
	}
}

bool UBasedMovementUtils::IsADynamicBase(const UPrimitiveComponent* MovementBase)
{
	return (MovementBase && MovementBase->Mobility == EComponentMobility::Movable);
}

bool UBasedMovementUtils::TryMoveToStayWithBase(USceneComponent* UpdatedComponent, UPrimitiveComponent* UpdatedPrimitive, const FRelativeBaseInfo& OldBaseInfo, FMovementRecord& MoveRecord, const bool& bIgnoreBaseRotation)
{
	FVector NewBaseLocation;
	FQuat NewBaseQuat;


	if (UBasedMovementUtils::GetMovementBaseTransform(OldBaseInfo.MovementBase, OldBaseInfo.BoneName, OUT NewBaseLocation, OUT NewBaseQuat))
	{
		const bool bDidBaseRotationChange = !OldBaseInfo.Rotation.Equals(NewBaseQuat, UE_SMALL_NUMBER);
		const bool bDidBaseLocationChange = (OldBaseInfo.Location != NewBaseLocation);

		// Find change in rotation
		FQuat DeltaQuat = FQuat::Identity;
		FVector DeltaLoc = FVector::ZeroVector;
		FQuat TargetQuat = UpdatedComponent->GetComponentQuat();

		if (bDidBaseRotationChange && !bIgnoreBaseRotation)
		{
			DeltaQuat = NewBaseQuat * OldBaseInfo.Rotation.Inverse();
			TargetQuat = DeltaQuat * TargetQuat;

			// TODO: make this respect the "up" direction, rather than assuming +Z = up
			FVector TargetForwVector = TargetQuat.GetForwardVector();
			TargetForwVector.Z = 0.f;
			TargetForwVector.Normalize();

			TargetQuat = UKismetMathLibrary::MakeRotFromX(TargetForwVector).Quaternion();
		}

		if (bDidBaseRotationChange || bDidBaseLocationChange)
		{
			// Calculate new transform matrix of base actor (ignoring scale).
			const FQuatRotationTranslationMatrix OldLocalToWorld(OldBaseInfo.Rotation, OldBaseInfo.Location);
			const FQuatRotationTranslationMatrix NewLocalToWorld(NewBaseQuat, NewBaseLocation);

			// Find change in location
			// NOTE that we need to use the floor hit location, not the actor's root position which may be floating above the base
			const FVector NewWorldBaseContactPos = NewLocalToWorld.TransformPosition(OldBaseInfo.ContactLocalPosition);

			const FVector OldWorldBaseContactPos = OldLocalToWorld.TransformPosition(OldBaseInfo.ContactLocalPosition);

			DeltaLoc = NewWorldBaseContactPos - OldWorldBaseContactPos;

			EMoveComponentFlags MoveComponentFlags = MOVECOMP_IgnoreBases;

			const bool bSweep = true;
			FHitResult MoveHitResult;
			
			bool bDidMove = UMovementUtils::TryMoveUpdatedComponent_Internal(UpdatedComponent, DeltaLoc, TargetQuat, bSweep, MoveComponentFlags, &MoveHitResult, ETeleportType::None);
			
			return bDidMove;
		}

		// TODO: rework this to ensure MoveRecord contains the appropriate thing
	}

	return false;
}

bool UBasedMovementUtils::GetMovementBaseTransform(const UPrimitiveComponent* MovementBase, const FName BoneName, FVector& OutLocation, FQuat& OutQuat)
{
	if (MovementBase)
	{
		bool bBoneNameIsInvalid = false;

		if (BoneName != NAME_None)
		{
			// Check if this socket or bone exists (DoesSocketExist checks for either, as does requesting the transform).
			if (MovementBase->DoesSocketExist(BoneName))
			{
				MovementBase->GetSocketWorldLocationAndRotation(BoneName, OutLocation, OutQuat);
				return true;
			}

			bBoneNameIsInvalid = true;
			UE_LOG(LogMover, Warning, TEXT("GetMovementBaseTransform(): Invalid bone or socket '%s' for PrimitiveComponent base %s. Using component's root transform instead."), *BoneName.ToString(), *GetPathNameSafe(MovementBase));
		}

		OutLocation = MovementBase->GetComponentLocation();
		OutQuat = MovementBase->GetComponentQuat();
		return !bBoneNameIsInvalid;
	}

	// nullptr MovementBase
	OutLocation = FVector::ZeroVector;
	OutQuat = FQuat::Identity;
	return false;
}


bool UBasedMovementUtils::TransformBasedLocationToWorld(const UPrimitiveComponent* MovementBase, const FName BoneName, FVector LocalLocation, FVector& OutLocationWorldSpace)
{
	FVector BaseLocation;
	FQuat BaseQuat;
	
	if (GetMovementBaseTransform(MovementBase, BoneName, /*out*/ BaseLocation, /*out*/ BaseQuat))
	{ 
		TransformLocationToWorld(BaseLocation, BaseQuat, LocalLocation, OutLocationWorldSpace);
		return true;
	}
	
	return false;
}


bool UBasedMovementUtils::TransformWorldLocationToBased(const UPrimitiveComponent* MovementBase, const FName BoneName, FVector WorldSpaceLocation, FVector& OutLocalLocation)
{
	FVector BaseLocation;
	FQuat BaseQuat;
	if (GetMovementBaseTransform(MovementBase, BoneName, /*out*/ BaseLocation, /*out*/ BaseQuat))
	{
		TransformLocationToLocal(BaseLocation, BaseQuat, WorldSpaceLocation, OutLocalLocation);
		return true;
	}

	return false;
}


bool UBasedMovementUtils::TransformBasedDirectionToWorld(const UPrimitiveComponent* MovementBase, const FName BoneName, FVector LocalDirection, FVector& OutDirectionWorldSpace)
{
	FVector IgnoredLocation;
	FQuat BaseQuat;
	if (GetMovementBaseTransform(MovementBase, BoneName, /*out*/ IgnoredLocation, /*out*/ BaseQuat))
	{
		TransformDirectionToWorld(BaseQuat, LocalDirection, OutDirectionWorldSpace);
		return true;
	}

	return false;
}


bool UBasedMovementUtils::TransformWorldDirectionToBased(const UPrimitiveComponent* MovementBase, const FName BoneName, FVector WorldSpaceDirection, FVector& OutLocalDirection)
{
	FVector IgnoredLocation;
	FQuat BaseQuat;
	if (GetMovementBaseTransform(MovementBase, BoneName, /*out*/ IgnoredLocation, /*out*/ BaseQuat))
	{
		TransformDirectionToLocal(BaseQuat, WorldSpaceDirection, OutLocalDirection);
		return true;
	}

	return false;
}


bool UBasedMovementUtils::TransformBasedRotatorToWorld(const UPrimitiveComponent* MovementBase, const FName BoneName, FRotator LocalRotator, FRotator& OutWorldSpaceRotator)
{
	FVector IgnoredLocation;
	FQuat BaseQuat;
	
	if (GetMovementBaseTransform(MovementBase, BoneName, /*out*/ IgnoredLocation, /*out*/ BaseQuat))
	{
		TransformRotatorToWorld(BaseQuat, LocalRotator, OutWorldSpaceRotator);
		return true;
	}

	return false;
}


bool UBasedMovementUtils::TransformWorldRotatorToBased(const UPrimitiveComponent* MovementBase, const FName BoneName, FRotator WorldSpaceRotator, FRotator& OutLocalRotator)
{
	FVector IgnoredLocation;
	FQuat BaseQuat;
	if (GetMovementBaseTransform(MovementBase, BoneName, /*out*/ IgnoredLocation, /*out*/ BaseQuat))
	{
		TransformRotatorToLocal(BaseQuat, WorldSpaceRotator, OutLocalRotator);
		return true;
	}
	return false;
}


void UBasedMovementUtils::TransformLocationToWorld(FVector BasePos, FQuat BaseQuat, FVector LocalLocation, FVector& OutLocationWorldSpace)
{
	OutLocationWorldSpace = FTransform(BaseQuat, BasePos).TransformPositionNoScale(LocalLocation);
}

void UBasedMovementUtils::TransformLocationToLocal(FVector BasePos, FQuat BaseQuat, FVector WorldSpaceLocation, FVector& OutLocalLocation)
{
	OutLocalLocation = FTransform(BaseQuat, BasePos).InverseTransformPositionNoScale(WorldSpaceLocation);
}

void UBasedMovementUtils::TransformDirectionToWorld(FQuat BaseQuat, FVector LocalDirection, FVector& OutDirectionWorldSpace)
{
	OutDirectionWorldSpace = BaseQuat.RotateVector(LocalDirection);
}

void UBasedMovementUtils::TransformDirectionToLocal(FQuat BaseQuat, FVector WorldSpaceDirection, FVector& OutLocalDirection)
{
	OutLocalDirection = BaseQuat.UnrotateVector(WorldSpaceDirection);
}

void UBasedMovementUtils::TransformRotatorToWorld(FQuat BaseQuat, FRotator LocalRotator, FRotator& OutWorldSpaceRotator)
{
	FQuat LocalQuat(LocalRotator);
	OutWorldSpaceRotator = (BaseQuat * LocalQuat).Rotator();
}

void UBasedMovementUtils::TransformRotatorToLocal(FQuat BaseQuat, FRotator WorldSpaceRotator, FRotator& OutLocalRotator)
{
	FQuat WorldQuat(WorldSpaceRotator);
	OutLocalRotator = (BaseQuat.Inverse() * WorldQuat).Rotator();
}
