// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Chaos/ParticleHandleFwd.h"
#include "Containers/Array.h"
#include "Math/MathFwd.h"

struct FFloorCheckResult;
struct FWaterCheckResult;
struct FHitResult;
class UPrimitiveComponent;

/**
 * PhysicsMovementUtils: a collection of stateless static functions for a variety of physics movement-related operations
 */
class MOVER_API UPhysicsMovementUtils
{
public:
	static void FindFloor(const FVector& Location, const FVector& DeltaPos, const UPrimitiveComponent* UpdatedPrimitive, const FVector& UpDir, float QueryRadius, float TargetHeight, float MaxStepHeight, float MaxWalkSlopeCosine, FFloorCheckResult& OutFloorResult, FWaterCheckResult& OutWaterResult);
	static const Chaos::FPBDRigidParticleHandle* GetRigidParticelHandleFromHitResult(const FHitResult& HitResult);

	// Checks if the hit surface is walkable and, if stepping up, whether the surface can be stepped up on
	static bool IsHitSurfaceWalkableWithStepUpCheck(const FHitResult& Hit, float StepHeight, float MaxStepHeight, float MinStepUpHeight, float MaxWalkSlopeCosine);

	// Checks if any hit is with water and, if so, fills in the OutWaterResult
	static bool GetWaterResultFromHitResults(const TArray<FHitResult>& Hits, const FVector& Location, FWaterCheckResult& OutWaterResult);

	static FVector ComputeGroundVelocityFromHitResult(const FVector& CharacterPosition, const FHitResult& FloorHit, const float DeltaSeconds);
};