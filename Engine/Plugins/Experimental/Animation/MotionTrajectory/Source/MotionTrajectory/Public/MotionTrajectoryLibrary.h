// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Curves/CurveFloat.h"
#include "PoseSearch/PoseSearchTrajectoryTypes.h"
#include "MotionTrajectoryLibrary.generated.h"

class ACharacter;
class UCharacterMovementComponent;

USTRUCT(BlueprintType)
struct MOTIONTRAJECTORY_API FTrajectorySamplingData
{
	GENERATED_BODY()

	void Init();

	// This should generally match the longest history required by a Motion Matching Database in the project.
	// Motion Matching will use extrapolation to generate samples if the history doesn't contain enough samples.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Trajectory Settings")
	float HistoryLengthSeconds = 1.5f;

	// Higher values will cost more storage and processing time, but give higher accuracy.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Trajectory Settings", meta = (ClampMin = "1", ClampMax = "120"))
	int32 HistorySamplesPerSecond = 5;

	// This should match the longest trajectory prediction required by a Motion Matching Database in the project.
	// Motion Matching will use extrapolation to generate samples if the prediction doesn't contain enough samples.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Trajectory Settings")
	float PredictionLengthSeconds = 1.5f;

	// Higher values will cost more storage and processing time, but give higher accuracy.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Trajectory Settings", meta = (ClampMin = "1", ClampMax = "120"))
	int32 PredictionSamplesPerSecond = 5;

	int32 NumHistorySamples = 0;
	float SecondsPerHistorySample = 0.f;

	int32 NumPredictionSamples = 0;
	float SecondsPerPredictionSample = 0.f;
};

USTRUCT(BlueprintType)
struct MOTIONTRAJECTORY_API FCharacterTrajectoryData
{
public:
	GENERATED_BODY()

	void Init(const AActor* Actor);
	void Update(float DeltaSeconds);

	bool IsValid() const;

	// If the character is forward facing (i.e. bOrientRotationToMovement is true), this controls how quickly the trajectory will rotate
	// to face acceleration. It's common for this to differ from the rotation rate of the character, because animations are often authored 
	// with different rotation speeds than the character. This is especially true in cases where the character rotation snaps to movement.
	UPROPERTY(EditDefaultsOnly, Category = "Trajectory Settings")
	float RotateTowardsMovementSpeed = 10.f;

	// Maximum controller rotation rate in degrees per second used to clamp the character owner controller desired rotation to generate the prediction trajectory.
	// Negative values disable the clamping behavior
	UPROPERTY(EditDefaultsOnly, Category = "Trajectory Settings")
	float MaxControllerRotationRate = -1.f;

	UPROPERTY(EditDefaultsOnly, Category = "Trajectory Settings", meta = (InlineEditConditionToggle))
	bool bUseSpeedRemappingCurve = false;

	UPROPERTY(EditDefaultsOnly, Category = "Trajectory Settings", meta = (EditCondition = "bUseSpeedRemappingCurve"))
	FRuntimeFloatCurve SpeedRemappingCurve;

	UPROPERTY(EditDefaultsOnly, Category = "Trajectory Settings", meta = (InlineEditConditionToggle))
	bool bUseAccelerationRemappingCurve = false;

	UPROPERTY(EditDefaultsOnly, Category = "Trajectory Settings", meta = (EditCondition = "bUseAccelerationRemappingCurve"))
	FRuntimeFloatCurve AccelerationRemappingCurve;

	UPROPERTY(Transient)
	TObjectPtr<const ACharacter> Character = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<const USkeletalMeshComponent> SkelMeshComponent = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<const UCharacterMovementComponent> CharacterMovementComponent = nullptr;

	FRotator ControllerRotationRate = FRotator::ZeroRotator;
	FRotator ControllerRotationRateClamped = FRotator::ZeroRotator;

private:
	void UpdateControllerRotationRate(float DeltaSeconds);

	FRotator DesiredControllerRotationLastUpdate = FRotator::ZeroRotator;
};

/**
 * Set of functions to help populate a FPoseSearchQueryTrajectory for motion matching.
 * UCharacterTrajectoryComponent uses these functions, but they can also be used by a UAnimInstance to avoid the component.
 */
struct MOTIONTRAJECTORY_API FMotionTrajectoryLibrary
{
public:
	static void InitTrajectorySamples(FPoseSearchQueryTrajectory& Trajectory,
		const FCharacterTrajectoryData& CharacterTrajectoryData, const FTrajectorySamplingData& SamplingData);

	// Update history by shifting world space samples back in time. This gives us a history of world space transforms.
	// This approach is simple, but it causes problems with moving platforms.
	static void UpdateHistory_ShiftInWorldSpace(FPoseSearchQueryTrajectory& Trajectory,
		const FTrajectorySamplingData& SamplingData, float DeltaSeconds);

	// Update history by tracking offsets that result from character intent (e.g. movement component velocity) and applying
	// that to the current world transform. This works well on moving platforms as it only stores a history of movement
	// that results from character intent.
	static void UpdateHistory_TransformHistory(FPoseSearchQueryTrajectory& Trajectory, TArrayView<FVector> TranslationHistory,
		const FCharacterTrajectoryData& CharacterTrajectoryData, const FTrajectorySamplingData& SamplingData, float DeltaSeconds);

	// Update prediction by simulating the movement math for ground locomotion in UCharacterMovementComponent.
	static void UpdatePrediction_SimulateCharacterMovement(FPoseSearchQueryTrajectory& Trajectory,
		const FCharacterTrajectoryData& CharacterTrajectoryData, const FTrajectorySamplingData& SamplingData, float DeltaSeconds);

private:
	static FVector RemapVectorMagnitudeWithCurve(const FVector& Vector, bool bUseCurve, const FRuntimeFloatCurve& Curve);
};