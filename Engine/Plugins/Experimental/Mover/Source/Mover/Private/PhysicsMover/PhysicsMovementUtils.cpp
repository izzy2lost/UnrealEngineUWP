// Copyright Epic Games, Inc. All Rights Reserved.

#include "PhysicsMover/PhysicsMovementUtils.h"
#include "Chaos/PhysicsObject.h"
#include "Chaos/PhysicsObjectInternalInterface.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "MoveLibrary/FloorQueryUtils.h"
#include "MoveLibrary/GroundMovementUtils.h"
#include "MoveLibrary/WaterMovementUtils.h"
#include "PhysicsMover/PhysicsMoverSimulationTypes.h"
#include "WaterBodyActor.h"

#if PHYSICSDRIVENMOTION_DEBUG_DRAW
#include "Chaos/DebugDrawQueue.h"
#endif

extern FPhysicsDrivenMotionDebugParams GPhysicsDrivenMotionDebugParams;

void UPhysicsMovementUtils::FindFloor(
	const FVector& Location,
	const FVector& DeltaPos,
	const UPrimitiveComponent* UpdatedPrimitive,
	const FVector& UpDir,
	float QueryRadius,
	float TargetHeight,
	float MaxStepHeight,
	float MaxWalkSlopeCosine,
	FFloorCheckResult& OutFloorResult,
	FWaterCheckResult& OutWaterResult)
{
	if (const UWorld* World = UpdatedPrimitive->GetWorld())
	{
		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(PhysicsFloorTest), false, UpdatedPrimitive->GetOwner());
		QueryParams.bTraceIntoSubComponents = false;
		const ECollisionChannel CollisionChannel = UpdatedPrimitive->GetCollisionObjectType();
		FCollisionResponseParams ResponseParams(ECR_Overlap);
		ResponseParams.CollisionResponse.SetResponse(ECC_WorldStatic, ECR_Block);
		ResponseParams.CollisionResponse.SetResponse(ECC_WorldDynamic, ECR_Block);
		ResponseParams.CollisionResponse.SetResponse(ECC_Vehicle, ECR_Block);
		ResponseParams.CollisionResponse.SetResponse(ECC_Destructible, ECR_Block);
		ResponseParams.CollisionResponse.SetResponse(ECC_PhysicsBody, ECR_Block);

		const FVector DeltaPosVert = DeltaPos.ProjectOnTo(UpDir);
		const FVector DeltaPosHoriz = DeltaPos - DeltaPosVert;

		const float StartOffset = TargetHeight - MaxStepHeight - QueryRadius;
		const float SweepDistance = MaxStepHeight + QueryRadius + FMath::Max(MaxStepHeight, -DeltaPosVert.Dot(UpDir));

		FVector Start = Location + DeltaPosHoriz - StartOffset * UpDir;
		FVector End = Start - SweepDistance * UpDir;
		TArray<FHitResult> Hits;
		FHitResult OutHit;
		if (World->SweepMultiByChannel(Hits, Start, End, FQuat::Identity, CollisionChannel, FCollisionShape::MakeSphere(QueryRadius), QueryParams, ResponseParams))
		{
			OutHit = Hits.Last();
		}

#if PHYSICSDRIVENMOTION_DEBUG_DRAW
		// Draw full length of query
		if (GPhysicsDrivenMotionDebugParams.DebugDrawGroundQueries)
		{
			const FVector Center = 0.5f * (Start + End);
			const float Dist = (Start - End).Size();
			Chaos::FDebugDrawQueue::GetInstance().DrawDebugCapsule(Center, 0.5f * Dist + QueryRadius, QueryRadius, FQuat::Identity, FColor::Silver, false, -1.f, 10, 1.0f);
		}
#endif

		if (OutHit.bBlockingHit)
		{
			const float Distance = UpDir.Dot(Location - OutHit.ImpactPoint);
			const float StepHeight = TargetHeight - Distance;
			const float MinStepHeight = 5.0f;

			bool bWalkable = IsHitSurfaceWalkableWithStepUpCheck(OutHit, StepHeight, MaxStepHeight, MinStepHeight, MaxWalkSlopeCosine);

#if PHYSICSDRIVENMOTION_DEBUG_DRAW
			if (GPhysicsDrivenMotionDebugParams.DebugDrawGroundQueries)
			{
				const FVector Center = Start - 0.5f * OutHit.Distance * UpDir;
				const FColor Color = bWalkable ? FColor::Green : FColor::Red;
				Chaos::FDebugDrawQueue::GetInstance().DrawDebugCapsule(Center, 0.5f * OutHit.Distance + QueryRadius, QueryRadius, FQuat::Identity, Color, false, -1.f, 10, 1.0f);
			}
#endif

			GetWaterResultFromHitResults(Hits, Location, OutWaterResult);

			if (bWalkable)
			{
				// Found a walkable surface
				OutFloorResult.bBlockingHit = true;
				OutFloorResult.bWalkableFloor = true;
				OutFloorResult.FloorDist = UpDir.Dot(Location - OutHit.ImpactPoint);;
				OutFloorResult.HitResult = OutHit;
			}
			else
			{
				// Hit something but not walkable. Fire some line queries to try and find a walkable surface

				bool bLineQueryWalkable = false;
				float LineQueryDistance = 0.0f;

				FVector MovementDir = FVector::ForwardVector;
				FVector PerpDir = FVector::RightVector;
				FVector Fwd = DeltaPos - DeltaPos.Dot(UpDir) * UpDir;
				const float FwdSizeSq = Fwd.SizeSquared();
				if (FwdSizeSq > UE_SMALL_NUMBER)
				{
					MovementDir = Fwd * FMath::InvSqrtEst(FwdSizeSq);
					PerpDir = UpDir.Cross(MovementDir);
				}

				const int NumLineTraces = 4;
				FVector StartPoints[NumLineTraces];

				StartPoints[0] = Start + QueryRadius * MovementDir;
				StartPoints[1] = Start - QueryRadius * MovementDir;
				StartPoints[2] = Start - QueryRadius * PerpDir;
				StartPoints[3] = Start + QueryRadius * PerpDir;

				FHitResult LineQueryHit[NumLineTraces];

				float MinDistance = SweepDistance + QueryRadius;
				int32 MinDistanceIdx = INDEX_NONE;
				for (int32 Idx = 0; Idx < NumLineTraces; ++Idx)
				{
					Start = StartPoints[Idx];
					End = Start - UpDir * SweepDistance;
					World->LineTraceSingleByChannel(LineQueryHit[Idx], Start, End, CollisionChannel, QueryParams);

					if (LineQueryHit[Idx].IsValidBlockingHit())
					{
						LineQueryDistance = UpDir.Dot(Location - LineQueryHit[Idx].ImpactPoint);
						const float LineQueryStepHeight = TargetHeight - LineQueryDistance;

						bLineQueryWalkable = IsHitSurfaceWalkableWithStepUpCheck(LineQueryHit[Idx], LineQueryStepHeight, MaxStepHeight, MinStepHeight, MaxWalkSlopeCosine);

						if (bLineQueryWalkable && LineQueryDistance < MinDistance)
						{
							MinDistance = LineQueryDistance;
							MinDistanceIdx = Idx;
						}
					}

#if PHYSICSDRIVENMOTION_DEBUG_DRAW
					if (GPhysicsDrivenMotionDebugParams.DebugDrawGroundQueries)
					{
						FVector Dir = End - Start;
						const float SizeSq = Dir.SizeSquared();
						if (SizeSq > UE_SMALL_NUMBER)
						{
							Dir *= FMath::InvSqrt(SizeSq);
						}

						if (LineQueryHit[Idx].bBlockingHit)
						{
							const FVector Center = Start + 0.5f * LineQueryHit[Idx].Distance * Dir;
							const FColor Color = bLineQueryWalkable ? FColor::Green : FColor::Red;
							Chaos::FDebugDrawQueue::GetInstance().DrawDebugLine(Start, LineQueryHit[Idx].ImpactPoint, Color, false, -1.f, 10, 1.0f);
						}
						else
						{
							const FVector Center = 0.5f * (Start + End);
							float Dist = SizeSq > UE_SMALL_NUMBER ? FMath::Sqrt(SizeSq) : 0.0f;
							Chaos::FDebugDrawQueue::GetInstance().DrawDebugLine(Start, End, FColor::Silver, false, -1.f, 10, 1.0f);
						}
					}
#endif
				}

				if (MinDistanceIdx != INDEX_NONE)
				{
					// Found a walkable surface
					OutFloorResult.bBlockingHit = true;
					OutFloorResult.bWalkableFloor = true;
					OutFloorResult.FloorDist = MinDistance;
					OutFloorResult.HitResult = LineQueryHit[MinDistanceIdx];
				}
				else
				{
					// Didn't find a walkable surface. Use original sphere query result
					OutFloorResult.bBlockingHit = true;
					OutFloorResult.bWalkableFloor = false;
					OutFloorResult.FloorDist = Distance;
					OutFloorResult.HitResult = OutHit;
				}

				// May have missed a water result due to the original query blocking on a non-walkable surface
				// so run again with no blocking results
				ResponseParams.CollisionResponse.SetAllChannels(ECR_Overlap);
				World->SweepMultiByChannel(Hits, Start, End, FQuat::Identity, CollisionChannel, FCollisionShape::MakeSphere(QueryRadius), QueryParams, ResponseParams);
				GetWaterResultFromHitResults(Hits, Location, OutWaterResult);
			}
		}
		else
		{
			// Sweep didn't hit anything blocking
			OutFloorResult.Clear();
			OutFloorResult.FloorDist = 1.0e10f;

			// In deep waters i.e., WaterDepth > SweepDistance, we may hit only water and no blocking surfaces
			GetWaterResultFromHitResults(Hits, Location, OutWaterResult);
		}
	}
}

bool UPhysicsMovementUtils::IsHitSurfaceWalkableWithStepUpCheck(const FHitResult& Hit, float StepHeight, float MaxStepHeight, float MinStepUpHeight, float MaxWalkSlopeCosine)
{
	bool bWalkable = false;
	if (StepHeight <= MaxStepHeight)
	{
		bWalkable = UFloorQueryUtils::IsHitSurfaceWalkable(Hit, MaxWalkSlopeCosine);
		if (bWalkable)
		{
			const float MinStepHeight = 1.0f;
			const bool bSteppingUp = StepHeight > MinStepHeight;
			if (bSteppingUp && !UGroundMovementUtils::CanStepUpOnHitSurface(Hit))
			{
				bWalkable = false;
			}
		}
	}

	return bWalkable;
}

const Chaos::FPBDRigidParticleHandle* UPhysicsMovementUtils::GetRigidParticelHandleFromHitResult(const FHitResult& HitResult)
{
	if (IPhysicsComponent* PhysicsComp = Cast<IPhysicsComponent>(HitResult.Component))
	{
		if (Chaos::FPhysicsObjectHandle PhysicsObject = PhysicsComp->GetPhysicsObjectById(HitResult.Item))
		{
			Chaos::FReadPhysicsObjectInterface_Internal Interface = Chaos::FPhysicsObjectInternalInterface::GetRead();
			if (const Chaos::FGeometryParticleHandle* Particle = Interface.GetParticle(PhysicsObject))
			{
				return Particle->CastToRigidParticle();
			}
		}
	}

	return nullptr;
}

FVector UPhysicsMovementUtils::ComputeGroundVelocityFromHitResult(const FVector& CharacterPosition, const FHitResult& FloorHit, const float DeltaSeconds)
{
	FVector GroundVelocity = FVector::ZeroVector;
	if (const Chaos::FPBDRigidParticleHandle* Rigid = GetRigidParticelHandleFromHitResult(FloorHit))
	{
		FVector Offset = CharacterPosition - Rigid->X();
		Offset -= Offset.ProjectOnToNormal(FloorHit.ImpactNormal);

		if (Rigid->KinematicTarget().IsSet())
		{
			const FVector LinearDisplacement = Rigid->KinematicTarget().GetTargetPosition() - Rigid->X();
			const FQuat RelativeQuat = Rigid->R().Inverse() * Rigid->KinematicTarget().GetTargetRotation();
			const FVector AngularDisplacement = RelativeQuat.ToRotationVector();
			GroundVelocity = (LinearDisplacement + AngularDisplacement.Cross(Offset)) / DeltaSeconds;
		}
		else
		{
			GroundVelocity = Rigid->V() + Rigid->W().Cross(Offset);
		}
	}
	return GroundVelocity;
}

bool UPhysicsMovementUtils::GetWaterResultFromHitResults(const TArray<FHitResult>& Hits, const FVector& Location, FWaterCheckResult& OutWaterResult)
{
	// Find the closet hit that is a water body
	// Note: Relies on ordering of hit results
	for (int32 Idx = 0; Idx < Hits.Num(); ++Idx)
	{
		const FHitResult& Hit = Hits[Idx];

		if (Hit.Component.IsValid())
		{
			if (const AActor* Actor = Hit.Component->GetOwner())
			{
				if (Actor->IsA(AWaterBody::StaticClass()))
				{
					OutWaterResult.HitResult = Hit;
					OutWaterResult.bSwimmableVolume = true;

					FWaterBodyQueryResult QueryResult = Cast<AWaterBody>(Actor)->GetWaterBodyComponent()->QueryWaterInfoClosestToWorldLocation(
						Location, EWaterBodyQueryFlags::ComputeDepth | EWaterBodyQueryFlags::ComputeVelocity | EWaterBodyQueryFlags::ComputeImmersionDepth);

					OutWaterResult.WaterSplineData.WaterDepth = QueryResult.GetWaterSurfaceDepth();
					OutWaterResult.WaterSplineData.RawWaterVelocity = QueryResult.GetVelocity();
					OutWaterResult.WaterSplineData.ImmersionDepth = QueryResult.GetImmersionDepth();

#if PHYSICSDRIVENMOTION_DEBUG_DRAW
					Chaos::FDebugDrawQueue::GetInstance().DrawDebugLine(Location, Location - FVector::UpVector * OutWaterResult.WaterSplineData.ImmersionDepth, FColor::Blue, false, -1.f, 10, 1.0f);
					Chaos::FDebugDrawQueue::GetInstance().DrawDebugPoint(OutWaterResult.HitResult.Location, FColor::Blue, false, -1.f, 10, 1.0f);
#endif
					return true;
				}
			}
		}
	}

	return false;
}