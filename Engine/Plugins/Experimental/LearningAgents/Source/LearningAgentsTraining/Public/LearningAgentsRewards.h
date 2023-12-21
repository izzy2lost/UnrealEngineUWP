// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"

#include "LearningAgentsRewards.generated.h"

class USplineComponent;

UCLASS(BlueprintType)
class LEARNINGAGENTSTRAINING_API ULearningAgentsRewards : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	// Rewards

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	static float RewardOnCondition(const bool bCondition, const float RewardScale = 1.0f);

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	static float RewardOnLocationDifferenceBelowThreshold(const FVector LocationA, const FVector LocationB, const float DistanceThreshold = 100.0f, const float RewardScale = 1.0f);

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	static float RewardFromLocationSimilarity(const FVector LocationA, const FVector LocationB, const float LocationScale = 100.0f, const float RewardScale = 1.0f);

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	static float RewardFromRotationSimilarity(const FRotator RotationA, const FRotator RotationB, const float AngleScale = 90.0f, const float RewardScale = 1.0f);
	static float RewardFromRotationSimilarityAsQuats(const FQuat RotationA, const FQuat RotationB, const float AngleScale = 90.0f, const float RewardScale = 1.0f);

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	static float RewardFromAngleSimilarity(const float AngleA, const float AngleB, const float AngleScale = 90.0f, const float RewardScale = 1.0f);

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	static float RewardFromDirectionSimilarity(const FVector DirectionA, const FVector DirectionB, const float AngleScale = 90.0f, const float RewardScale = 1.0f);

	// Spline Rewards

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	static float RewardFromVelocityAlongSpline(const USplineComponent* SplineComponent, const FVector Position, const FVector Velocity, const float VelocityScale = 200.0f, const float RewardScale = 1.0f, const float FiniteDifferenceDelta = 10.0f);

	// Penalties

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	static float PenaltyOnCondition(const bool bCondition, const float PenaltyScale = 1.0f);

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	static float PenaltyOnLocationDifferenceAboveThreshold(const FVector LocationA, const FVector LocationB, const float DistanceThreshold = 100.0f, const float PenaltyScale = 1.0f);

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	static float PenaltyFromLocationDifference(const FVector LocationA, const FVector LocationB, const float LocationScale = 100.0f, const float PenaltyScale = 1.0f);

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	static float PenaltyFromAngleDifference(const float AngleA, const float AngleB, const float AngleScale = 90.0f, const float PenaltyScale = 1.0f);

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	static float PenaltyFromRotationDifference(const FRotator RotationA, const FRotator RotationB, const float AngleScale = 90.0f, const float PenaltyScale = 1.0f);
	static float PenaltyFromRotationDifferenceAsQuats(const FQuat RotationA, const FQuat RotationB, const float AngleScale = 90.0f, const float PenaltyScale = 1.0f);

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	static float PenaltyFromDirectionDifference(const FVector DirectionA, const FVector DirectionB, const float AngleScale = 90.0f, const float PenaltyScale = 1.0f);


};
