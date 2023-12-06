// Copyright Epic Games, Inc. All Rights Reserved.

#include "LearningAgentsRewards.h"

#include "LearningLog.h"

#include "Components/SplineComponent.h"

float ULearningAgentsRewards::RewardOnCondition(const bool bCondition, const float RewardScale)
{
	return bCondition ? RewardScale : 0.0f;
}

float ULearningAgentsRewards::RewardOnTranslationDifferenceBelowThreshold(const FVector TranslationA, const FVector TranslationB, const float DistanceThreshold, const float RewardScale)
{
	return RewardOnCondition(FVector::Distance(TranslationA, TranslationB) < DistanceThreshold, RewardScale);
}

float ULearningAgentsRewards::RewardFromTranslationSimilarity(const FVector TranslationA, const FVector TranslationB, const float TranslationScale, const float RewardScale)
{
	const float TranslationDifference = FVector::Dist(TranslationA, TranslationB);
	return RewardScale * FMath::InvExpApprox(FMath::Square(TranslationDifference / FMath::Max(TranslationScale, UE_SMALL_NUMBER)));
}

float ULearningAgentsRewards::RewardFromAngleSimilarity(const float AngleA, const float AngleB, const float AngleScale, const float RewardScale)
{
	const float AngleDifference = FMath::FindDeltaAngleRadians(FMath::DegreesToRadians(AngleA), FMath::DegreesToRadians(AngleB));
	return RewardScale * FMath::InvExpApprox(FMath::Square(AngleDifference / FMath::Max(FMath::DegreesToRadians(AngleScale), UE_SMALL_NUMBER)));
}

float ULearningAgentsRewards::RewardFromRotationSimilarityAsQuats(const FQuat RotationA, const FQuat RotationB, const float AngleScale, const float RewardScale)
{
	FQuat Difference = RotationA.Inverse() * RotationB;
	Difference.EnforceShortestArcWith(FQuat::Identity);
	return RewardScale * FMath::InvExpApprox(FMath::Square(Difference.GetAngle() / (FMath::Max(FMath::DegreesToRadians(AngleScale), UE_SMALL_NUMBER))));
}

float ULearningAgentsRewards::RewardFromRotationSimilarity(const FRotator RotationA, const FRotator RotationB, const float AngleScale, const float RewardScale)
{
	return RewardScale * RewardFromRotationSimilarityAsQuats(RotationA.Quaternion(), RotationB.Quaternion(), AngleScale);
}

float ULearningAgentsRewards::RewardFromDirectionSimilarity(const FVector DirectionA, const FVector DirectionB, const float AngleScale, const float RewardScale)
{
	const float AngleDifference = FMath::Acos(DirectionA.Dot(DirectionB));
	return RewardScale * FMath::InvExpApprox(FMath::Square(AngleDifference / FMath::Max(FMath::DegreesToRadians(AngleScale), UE_SMALL_NUMBER)));
}

// Spline Rewards

float ULearningAgentsRewards::RewardFromVelocityAlongSpline(const USplineComponent* SplineComponent, const FVector Position, const FVector Velocity, const float VelocityScale, const float RewardScale, const float FiniteDifferenceDelta)
{
	if (!SplineComponent)
	{
		UE_LOG(LogLearning, Error, TEXT("RewardFromVelocityAlongSpline: SplineComponent is nullptr."));
		return 0.0f;
	}

	float FiniteDiff = FiniteDifferenceDelta;

	if (FiniteDiff < UE_KINDA_SMALL_NUMBER)
	{
		UE_LOG(LogLearning, Warning, TEXT("RewardFromVelocityAlongSpline: FiniteDifferenceDelta is too small (%6.4f). Clamping to %6.4f."), FiniteDifferenceDelta, UE_KINDA_SMALL_NUMBER);
		FiniteDiff = UE_KINDA_SMALL_NUMBER;
	}

	const float RawDistance0 = SplineComponent->GetDistanceAlongSplineAtLocation(Position, ESplineCoordinateSpace::World);
	const float RawDistance1 = SplineComponent->GetDistanceAlongSplineAtLocation(Position + FiniteDiff * Velocity.GetSafeNormal(), ESplineCoordinateSpace::World);

	float Distance0 = RawDistance0, Distance1 = RawDistance1;

	if (SplineComponent->IsClosedLoop())
	{
		const float SplineDistance = SplineComponent->GetSplineLength();

		if (FMath::Abs(Distance0 - (Distance1 + SplineDistance)) < FMath::Abs(Distance0 - Distance1))
		{
			Distance1 = Distance1 + SplineDistance;
		}
		else if (FMath::Abs((Distance0 + SplineDistance) - Distance1) < FMath::Abs(Distance0 - Distance1))
		{
			Distance0 = Distance0 + SplineDistance;
		}
	}

	return RewardScale * ((Distance1 - Distance0) / FiniteDiff) * (Velocity.Length() / FMath::Max(VelocityScale, UE_SMALL_NUMBER));
}


// Penalties

float ULearningAgentsRewards::PenaltyOnCondition(const bool bCondition, const float PenaltyScale)
{
	return bCondition ? -PenaltyScale : 0.0f;
}

float ULearningAgentsRewards::PenaltyOnTranslationDifferenceAboveThreshold(const FVector TranslationA, const FVector TranslationB, const float DistanceThreshold, const float PenaltyScale)
{
	return PenaltyOnCondition(FVector::Distance(TranslationA, TranslationB) > DistanceThreshold, PenaltyScale);
}

float ULearningAgentsRewards::PenaltyFromTranslationDifference(const FVector TranslationA, const FVector TranslationB, const float TranslationScale, const float PenaltyScale)
{
	const float TranslationDifference = FVector::Dist(TranslationA, TranslationB);
	return PenaltyScale  * (-TranslationDifference / FMath::Max(TranslationScale, UE_SMALL_NUMBER));
}

float ULearningAgentsRewards::PenaltyFromAngleDifference(const float AngleA, const float AngleB, const float AngleScale, const float PenaltyScale)
{
	const float AngleDifference = FMath::FindDeltaAngleRadians(FMath::DegreesToRadians(AngleA), FMath::DegreesToRadians(AngleB));
	return PenaltyScale  * (-AngleDifference / FMath::Max(FMath::DegreesToRadians(AngleScale), UE_SMALL_NUMBER));
}

float ULearningAgentsRewards::PenaltyFromRotationDifferenceAsQuats(const FQuat RotationA, const FQuat RotationB, const float AngleScale, const float PenaltyScale)
{
	FQuat Difference = RotationA.Inverse() * RotationB;
	Difference.EnforceShortestArcWith(FQuat::Identity);
	return PenaltyScale  * (-Difference.GetAngle() / (FMath::Max(FMath::DegreesToRadians(AngleScale), UE_SMALL_NUMBER)));
}

float ULearningAgentsRewards::PenaltyFromRotationDifference(const FRotator RotationA, const FRotator RotationB, const float AngleScale, const float PenaltyScale)
{
	return PenaltyFromRotationDifferenceAsQuats(RotationA.Quaternion(), RotationB.Quaternion(), AngleScale, PenaltyScale);
}

float ULearningAgentsRewards::PenaltyFromDirectionDifference(const FVector DirectionA, const FVector DirectionB, const float AngleScale, const float PenaltyScale)
{
	const float AngleDifference = FMath::Acos(DirectionA.Dot(DirectionB));
	return PenaltyScale  * (-AngleDifference / FMath::Max(FMath::DegreesToRadians(AngleScale), UE_SMALL_NUMBER));
}

