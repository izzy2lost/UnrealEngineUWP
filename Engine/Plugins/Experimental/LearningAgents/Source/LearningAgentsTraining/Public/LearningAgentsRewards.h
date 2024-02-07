// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"

#include "LearningAgentsRewards.generated.h"

class ULearningAgentsManagerListener;
class USplineComponent;

UCLASS(BlueprintType)
class LEARNINGAGENTSTRAINING_API ULearningAgentsRewards : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	/**
	 * Make a reward from a float value.
	 *
	 * @param RewardValue The float value used to create the reward.
	 * @param RewardScale The scale of the reward. Use a negative scale to create a penalty.
	 * @param Name The name of the reward. Used for debugging.
	 * @param bVisualLoggerEnabled When true, debug data will be sent to the visual logger.
	 * @param VisualLoggerListener The listener object which is making this reward. This must be set to use logging.
	 * @param VisualLoggerAgentId The agent id associated with this reward.
	 * @param VisualLoggerLocation A location for the visual logger information in the world.
	 * @param VisualLoggerColor The color for the visual logger display.
	 * @return The resulting reward value.
	 */
	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 3, DefaultToSelf = "VisualLoggerListener"))
	static float MakeReward(
		const float RewardValue, 
		const float RewardScale = 1.0f, 
		const FName Name = TEXT("Reward"),
		const bool bVisualLoggerEnabled = false,
		ULearningAgentsManagerListener* VisualLoggerListener = nullptr,
		const int32 VisualLoggerAgentId = -1,
		const FVector VisualLoggerLocation = FVector::ZeroVector,
		const FLinearColor VisualLoggerColor = FLinearColor::Green);

	/**
	 * Make a reward which is equal to RewardScale when bCondition is true, otherwise returns zero.
	 *
	 * @param bCondition The condition under which to create a reward.
	 * @param RewardScale The scale of the reward. Use a negative scale to create a penalty.
	 * @param Name The name of the reward. Used for debugging.
	 * @param bVisualLoggerEnabled When true, debug data will be sent to the visual logger.
	 * @param VisualLoggerListener The listener object which is making this reward. This must be set to use logging.
	 * @param VisualLoggerAgentId The agent id associated with this reward.
	 * @param VisualLoggerLocation A location for the visual logger information in the world.
	 * @param VisualLoggerColor The color for the visual logger display.
	 * @return The resulting reward value.
	 */
	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 3, DefaultToSelf = "VisualLoggerListener"))
	static float MakeRewardOnCondition(
		const bool bCondition, 
		const float RewardScale = 1.0f,
		const FName Name = TEXT("RewardOnCondition"),
		const bool bVisualLoggerEnabled = false,
		ULearningAgentsManagerListener* VisualLoggerListener = nullptr,
		const int32 VisualLoggerAgentId = -1,
		const FVector VisualLoggerLocation = FVector::ZeroVector,
		const FLinearColor VisualLoggerColor = FLinearColor::Green);

	/**
	 * Make a reward when the distance between two locations is below a threshold, otherwise returns zero.
	 *
	 * @param LocationA The first location.
	 * @param LocationB The second location.
	 * @param DistanceThreshold The distance threshold.
	 * @param RewardScale The scale of the reward. Use a negative scale to create a penalty.
	 * @param Name The name of the reward. Used for debugging.
	 * @param bVisualLoggerEnabled When true, debug data will be sent to the visual logger.
	 * @param VisualLoggerListener The listener object which is making this reward. This must be set to use logging.
	 * @param VisualLoggerAgentId The agent id associated with this reward.
	 * @param VisualLoggerLocation A location for the visual logger information in the world.
	 * @param VisualLoggerColor The color for the visual logger display.
	 * @return The resulting reward value.
	 */
	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 5, DefaultToSelf = "VisualLoggerListener"))
	static float MakeRewardOnLocationDifferenceBelowThreshold(
		const FVector LocationA, 
		const FVector LocationB, 
		const float DistanceThreshold = 100.0f, 
		const float RewardScale = 1.0f,
		const FName Name = TEXT("RewardOnLocationDifferenceBelowThreshold"),
		const bool bVisualLoggerEnabled = false,
		ULearningAgentsManagerListener* VisualLoggerListener = nullptr,
		const int32 VisualLoggerAgentId = -1,
		const FVector VisualLoggerLocation = FVector::ZeroVector,
		const FLinearColor VisualLoggerColor = FLinearColor::Green);

	/**
	 * Make a reward when the distance between two locations is above a threshold, otherwise returns zero.
	 *
	 * @param LocationA The first location.
	 * @param LocationB The second location.
	 * @param DistanceThreshold The distance threshold.
	 * @param RewardScale The scale of the reward. Use a negative scale to create a penalty.
	 * @param Name The name of the reward. Used for debugging.
	 * @param bVisualLoggerEnabled When true, debug data will be sent to the visual logger.
	 * @param VisualLoggerListener The listener object which is making this reward. This must be set to use logging.
	 * @param VisualLoggerAgentId The agent id associated with this reward.
	 * @param VisualLoggerLocation A location for the visual logger information in the world.
	 * @param VisualLoggerColor The color for the visual logger display.
	 * @return The resulting reward value.
	 */
	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 5, DefaultToSelf = "VisualLoggerListener"))
	static float MakeRewardOnLocationDifferenceAboveThreshold(
		const FVector LocationA,
		const FVector LocationB,
		const float DistanceThreshold = 100.0f,
		const float RewardScale = 1.0f,
		const FName Name = TEXT("RewardOnLocationDifferenceAboveThreshold"),
		const bool bVisualLoggerEnabled = false,
		ULearningAgentsManagerListener* VisualLoggerListener = nullptr,
		const int32 VisualLoggerAgentId = -1,
		const FVector VisualLoggerLocation = FVector::ZeroVector,
		const FLinearColor VisualLoggerColor = FLinearColor::Green);

	/**
	 * Make a reward based on how similar two locations are.
	 *
	 * @param LocationA The first location.
	 * @param LocationB The second location.
	 * @param LocationScale The expected scale for the distance between locations.
	 * @param RewardScale The scale of the reward. Use a negative scale to create a penalty.
	 * @param Name The name of the reward. Used for debugging.
	 * @param bVisualLoggerEnabled When true, debug data will be sent to the visual logger.
	 * @param VisualLoggerListener The listener object which is making this reward. This must be set to use logging.
	 * @param VisualLoggerAgentId The agent id associated with this reward.
	 * @param VisualLoggerLocation A location for the visual logger information in the world.
	 * @param VisualLoggerColor The color for the visual logger display.
	 * @return The resulting reward value.
	 */
	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 5, DefaultToSelf = "VisualLoggerListener"))
	static float MakeRewardFromLocationSimilarity(
		const FVector LocationA, 
		const FVector LocationB, 
		const float LocationScale = 100.0f, 
		const float RewardScale = 1.0f,
		const FName Name = TEXT("RewardFromLocationSimilarity"),
		const bool bVisualLoggerEnabled = false,
		ULearningAgentsManagerListener* VisualLoggerListener = nullptr,
		const int32 VisualLoggerAgentId = -1,
		const FVector VisualLoggerLocation = FVector::ZeroVector,
		const FLinearColor VisualLoggerColor = FLinearColor::Green);

	/**
	 * Make a reward based on how similar two rotations are.
	 *
	 * @param RotationA The first rotation.
	 * @param RotationB The second rotation.
	 * @param AngleScale The expected scale for the angle between rotations.
	 * @param RewardScale The scale of the reward. Use a negative scale to create a penalty.
	 * @param Name The name of the reward. Used for debugging.
	 * @param bVisualLoggerEnabled When true, debug data will be sent to the visual logger.
	 * @param VisualLoggerListener The listener object which is making this reward. This must be set to use logging.
	 * @param VisualLoggerAgentId The agent id associated with this reward.
	 * @param VisualLoggerRotationLocationA A location for the visual logger to display the first rotation in the world.
	 * @param VisualLoggerRotationLocationB A location for the visual logger to display the second rotation in the world.
	 * @param VisualLoggerLocation A location for the visual logger information in the world.
	 * @param VisualLoggerColor The color for the visual logger display.
	 * @return The resulting reward value.
	 */
	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 5, DefaultToSelf = "VisualLoggerListener"))
	static float MakeRewardFromRotationSimilarity(
		const FRotator RotationA, 
		const FRotator RotationB, 
		const float AngleScale = 90.0f, 
		const float RewardScale = 1.0f,
		const FName Name = TEXT("RewardFromRotationSimilarity"),
		const bool bVisualLoggerEnabled = false,
		ULearningAgentsManagerListener* VisualLoggerListener = nullptr,
		const int32 VisualLoggerAgentId = -1,
		const FVector VisualLoggerRotationLocationA = FVector::ZeroVector,
		const FVector VisualLoggerRotationLocationB = FVector::ZeroVector,
		const FVector VisualLoggerLocation = FVector::ZeroVector,
		const FLinearColor VisualLoggerColor = FLinearColor::Green);

	/**
	 * Make a reward based on how similar two rotations are represented as quaternions.
	 *
	 * @param RotationA The first rotation.
	 * @param RotationB The second rotation.
	 * @param AngleScale The expected scale for the angle between rotations.
	 * @param RewardScale The scale of the reward. Use a negative scale to create a penalty.
	 * @param Name The name of the reward. Used for debugging.
	 * @param bVisualLoggerEnabled When true, debug data will be sent to the visual logger.
	 * @param VisualLoggerListener The listener object which is making this reward. This must be set to use logging.
	 * @param VisualLoggerAgentId The agent id associated with this reward.
	 * @param VisualLoggerRotationLocationA A location for the visual logger to display the first rotation in the world.
	 * @param VisualLoggerRotationLocationB A location for the visual logger to display the second rotation in the world.
	 * @param VisualLoggerLocation A location for the visual logger information in the world.
	 * @param VisualLoggerColor The color for the visual logger display.
	 * @return The resulting reward value.
	 */
	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 5, DefaultToSelf = "VisualLoggerListener"))
	static float MakeRewardFromRotationSimilarityAsQuats(
		const FQuat RotationA, 
		const FQuat RotationB, 
		const float AngleScale = 90.0f,
		const float RewardScale = 1.0f,
		const FName Name = TEXT("RewardFromRotationSimilarity"),
		const bool bVisualLoggerEnabled = false,
		ULearningAgentsManagerListener* VisualLoggerListener = nullptr,
		const int32 VisualLoggerAgentId = -1,
		const FVector VisualLoggerRotationLocationA = FVector::ZeroVector,
		const FVector VisualLoggerRotationLocationB = FVector::ZeroVector,
		const FVector VisualLoggerLocation = FVector::ZeroVector,
		const FLinearColor VisualLoggerColor = FLinearColor::Green);

	/**
	 * Make a reward based on how similar two angles are.
	 *
	 * @param AngleA The first angle.
	 * @param AngleB The second angle.
	 * @param AngleScale The expected scale for the angle between angles.
	 * @param RewardScale The scale of the reward. Use a negative scale to create a penalty.
	 * @param Name The name of the reward. Used for debugging.
	 * @param bVisualLoggerEnabled When true, debug data will be sent to the visual logger.
	 * @param VisualLoggerListener The listener object which is making this reward. This must be set to use logging.
	 * @param VisualLoggerAgentId The agent id associated with this reward.
	 * @param VisualLoggerAngleLocationA A location for the visual logger to display the first angle in the world.
	 * @param VisualLoggerAngleLocationB A location for the visual logger to display the second angle in the world.
	 * @param VisualLoggerLocation A location for the visual logger information in the world.
	 * @param VisualLoggerColor The color for the visual logger display.
	 * @return The resulting reward value.
	 */
	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 5, DefaultToSelf = "VisualLoggerListener"))
	static float MakeRewardFromAngleSimilarity(
		const float AngleA, 
		const float AngleB, 
		const float AngleScale = 90.0f, 
		const float RewardScale = 1.0f,
		const FName Name = TEXT("RewardFromAngleSimilarity"),
		const bool bVisualLoggerEnabled = false,
		ULearningAgentsManagerListener* VisualLoggerListener = nullptr,
		const int32 VisualLoggerAgentId = -1,
		const FVector VisualLoggerAngleLocationA = FVector::ZeroVector,
		const FVector VisualLoggerAngleLocationB = FVector::ZeroVector,
		const FVector VisualLoggerLocation = FVector::ZeroVector,
		const FLinearColor VisualLoggerColor = FLinearColor::Green);

	/**
	 * Make a reward based on how similar two directions are.
	 *
	 * @param DirectionA The first angle.
	 * @param DirectionB The second angle.
	 * @param AngleScale The expected scale for the angle between the directions.
	 * @param RewardScale The scale of the reward. Use a negative scale to create a penalty.
	 * @param Name The name of the reward. Used for debugging.
	 * @param bVisualLoggerEnabled When true, debug data will be sent to the visual logger.
	 * @param VisualLoggerListener The listener object which is making this reward. This must be set to use logging.
	 * @param VisualLoggerAgentId The agent id associated with this reward.
	 * @param VisualLoggerDirectionLocationA A location for the visual logger to display the first direction in the world.
	 * @param VisualLoggerDirectionLocationB A location for the visual logger to display the second direction in the world.
	 * @param VisualLoggerLocation A location for the visual logger information in the world.
	 * @param VisualLoggerArrowLength The length of the arrow to display for the directions.
	 * @param VisualLoggerColor The color for the visual logger display.
	 * @return The resulting reward value.
	 */
	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 5, DefaultToSelf = "VisualLoggerListener", DirectionA="1.0,0.0,0.0", DirectionB = "1.0,0.0,0.0"))
	static float MakeRewardFromDirectionSimilarity(
		const FVector DirectionA, 
		const FVector DirectionB, 
		const float AngleScale = 90.0f, 
		const float RewardScale = 1.0f,
		const FName Name = TEXT("RewardFromDirectionSimilarity"),
		const bool bVisualLoggerEnabled = false,
		ULearningAgentsManagerListener* VisualLoggerListener = nullptr,
		const int32 VisualLoggerAgentId = -1,
		const FVector VisualLoggerDirectionLocationA = FVector::ZeroVector,
		const FVector VisualLoggerDirectionLocationB = FVector::ZeroVector,
		const FVector VisualLoggerLocation = FVector::ZeroVector,
		const float VisualLoggerArrowLength = 100.0f,
		const FLinearColor VisualLoggerColor = FLinearColor::Green);

	/**
	 * Make a reward based on the velocity an object is moving along a spline.
	 *
	 * @param SplineComponent The spline.
	 * @param Location The location of the object.
	 * @param Velocity The velocity of the object.
	 * @param VelocityScale The expected scale for the velocity.
	 * @param RewardScale The scale of the reward. Use a negative scale to create a penalty.
	 * @param FiniteDifferenceDelta The finite difference to use when computing the velocity along the spline.
	 * @param Name The name of the reward. Used for debugging.
	 * @param bVisualLoggerEnabled When true, debug data will be sent to the visual logger.
	 * @param VisualLoggerListener The listener object which is making this reward. This must be set to use logging.
	 * @param VisualLoggerAgentId The agent id associated with this reward.
	 * @param VisualLoggerLocation A location for the visual logger information in the world.
	 * @param VisualLoggerColor The color for the visual logger display.
	 * @return The resulting reward value.
	 */
	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 7, DefaultToSelf = "VisualLoggerListener"))
	static float MakeRewardFromVelocityAlongSpline(
		const USplineComponent* SplineComponent, 
		const FVector Location, 
		const FVector Velocity, 
		const float VelocityScale = 200.0f, 
		const float RewardScale = 1.0f, 
		const float FiniteDifferenceDelta = 10.0f,
		const FName Name = TEXT("RewardFromVelocityAlongSpline"),
		const bool bVisualLoggerEnabled = false,
		ULearningAgentsManagerListener* VisualLoggerListener = nullptr,
		const int32 VisualLoggerAgentId = -1,
		const FVector VisualLoggerLocation = FVector::ZeroVector,
		const FLinearColor VisualLoggerColor = FLinearColor::Green);
};
