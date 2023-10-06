// Copyright Epic Games, Inc. All Rights Reserved.

#include "MotionTrajectoryLibrary.h"
#include "CharacterMovementTrajectoryLibrary.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

void FTrajectorySamplingData::Init()
{
	// The UI clamps these to be non-zero.
	check(HistorySamplesPerSecond);
	check(PredictionSamplesPerSecond);

	NumHistorySamples = FMath::CeilToInt32(HistoryLengthSeconds * HistorySamplesPerSecond);
	SecondsPerHistorySample = 1.f / HistorySamplesPerSecond;

	NumPredictionSamples = FMath::CeilToInt32(PredictionLengthSeconds * PredictionSamplesPerSecond);
	SecondsPerPredictionSample = 1.f / PredictionSamplesPerSecond;
}

void FCharacterTrajectoryData::Init(const AActor* Actor)
{
	Character = Cast<ACharacter>(Actor);
	if (!ensureMsgf(Character, TEXT("FCharacterTrajectoryData requires valid ACharacter owner.")))
	{
		return;
	}

	SkelMeshComponent = Character->GetMesh();
	if (!ensureMsgf(SkelMeshComponent, TEXT("FCharacterTrajectoryData must be run on an ACharacter with a valid USkeletalMeshComponent.")))
	{
		return;
	}

	CharacterMovementComponent = Character->GetCharacterMovement();
	if (!ensureMsgf(CharacterMovementComponent, TEXT("FCharacterTrajectoryData must be run on an ACharacter with a valid UCharacterMovementComponent.")))
	{
		return;
	}
}

void FCharacterTrajectoryData::Update(float DeltaSeconds)
{
	UpdateControllerRotationRate(DeltaSeconds);
}

bool FCharacterTrajectoryData::IsValid() const
{
	// Init() won't initialize CharacterMovementComponent if Character or SkelMeshComponent are null.
	check(!CharacterMovementComponent || (Character && SkelMeshComponent));

	return (CharacterMovementComponent != nullptr);
}

void FCharacterTrajectoryData::UpdateControllerRotationRate(float DeltaSeconds)
{
	ControllerRotationRate = FRotator::ZeroRotator;

	if (!ensure(DeltaSeconds > 0.f))
	{
		return;
	}

	if (!ensure(IsValid()))
	{
		return;
	}

	const AController* Controller = Character->Controller;
	if (!Controller)
	{
		// @todo: Simulated proxies don't have controllers, so they'll need some other mechanism to account for controller rotation rate.
		return;
	}

	FRotator DesiredControllerRotation = Controller->GetDesiredRotation();
	if (CharacterMovementComponent->ShouldRemainVertical())
	{
		DesiredControllerRotation.Yaw = FRotator::NormalizeAxis(DesiredControllerRotation.Yaw);
		DesiredControllerRotation.Pitch = 0.f;
		DesiredControllerRotation.Roll = 0.f;
	}

	const FRotator DesiredRotationDelta = DesiredControllerRotation - DesiredControllerRotationLastUpdate;
	DesiredControllerRotationLastUpdate = DesiredControllerRotation;

	ControllerRotationRate = DesiredRotationDelta.GetNormalized() * (1.f / DeltaSeconds);
	if (MaxControllerRotationRate >= 0.f)
	{
		ControllerRotationRate.Pitch = FMath::Sign(ControllerRotationRate.Pitch) * FMath::Min(FMath::Abs(ControllerRotationRate.Pitch), MaxControllerRotationRate);
		ControllerRotationRate.Yaw = FMath::Sign(ControllerRotationRate.Yaw) * FMath::Min(FMath::Abs(ControllerRotationRate.Yaw), MaxControllerRotationRate);
		ControllerRotationRate.Roll = FMath::Sign(ControllerRotationRate.Roll) * FMath::Min(FMath::Abs(ControllerRotationRate.Roll), MaxControllerRotationRate);
	}
}

void FMotionTrajectoryLibrary::InitTrajectorySamples(FPoseSearchQueryTrajectory& Trajectory,
	const FCharacterTrajectoryData& CharacterTrajectoryData, const FTrajectorySamplingData& SamplingData)
{
	if (!ensure(CharacterTrajectoryData.SkelMeshComponent))
	{
		return;
	}

	const FVector PositionWS = CharacterTrajectoryData.SkelMeshComponent->GetComponentLocation();;
	const FQuat FacingWS = CharacterTrajectoryData.SkelMeshComponent->GetComponentRotation().Quaternion();

	// History + current sample + prediction
	Trajectory.Samples.SetNumUninitialized(SamplingData.NumHistorySamples + 1 + SamplingData.NumPredictionSamples);

	// Initialize history samples
	for (int32 i = 0; i < SamplingData.NumHistorySamples; ++i)
	{
		Trajectory.Samples[i].Position = PositionWS;
		Trajectory.Samples[i].Facing = FacingWS;
		Trajectory.Samples[i].AccumulatedSeconds = SamplingData.SecondsPerHistorySample * (i - SamplingData.NumHistorySamples);
	}

	// Initialize current sample and prediction
	for (int32 i = SamplingData.NumHistorySamples; i < Trajectory.Samples.Num(); ++i)
	{
		Trajectory.Samples[i].Position = PositionWS;
		Trajectory.Samples[i].Facing = FacingWS;
		Trajectory.Samples[i].AccumulatedSeconds = SamplingData.SecondsPerPredictionSample * (i - SamplingData.NumHistorySamples);
	}
}

void FMotionTrajectoryLibrary::UpdateHistory_ShiftInWorldSpace(FPoseSearchQueryTrajectory& Trajectory,
	const FTrajectorySamplingData& SamplingData, float DeltaSeconds)
{
	check(SamplingData.NumHistorySamples <= Trajectory.Samples.Num());

	// Shift history Samples when it's time to record a new one.
	if (SamplingData.NumHistorySamples > 0 && FMath::Abs(Trajectory.Samples[SamplingData.NumHistorySamples - 1].AccumulatedSeconds) >= SamplingData.SecondsPerHistorySample)
	{
		for (int32 Index = 0; Index < SamplingData.NumHistorySamples; ++Index)
		{
			Trajectory.Samples[Index] = Trajectory.Samples[Index + 1];
			Trajectory.Samples[Index].AccumulatedSeconds -= DeltaSeconds;
		}
	}
	else
	{
		for (int32 Index = 0; Index < SamplingData.NumHistorySamples; ++Index)
		{
			Trajectory.Samples[Index].AccumulatedSeconds -= DeltaSeconds;
		}
	}
}

void FMotionTrajectoryLibrary::UpdateHistory_TransformHistory(FPoseSearchQueryTrajectory& Trajectory, TArrayView<FVector> TranslationHistory,
	const FCharacterTrajectoryData& CharacterTrajectoryData, const FTrajectorySamplingData& SamplingData, float DeltaSeconds)
{
	check(SamplingData.NumHistorySamples <= Trajectory.Samples.Num());
	check(TranslationHistory.Num() == SamplingData.NumHistorySamples);

	const FVector& CurrentVelocityWS = CharacterTrajectoryData.CharacterMovementComponent->Velocity;
	FVector CurrentTranslation = CurrentVelocityWS * DeltaSeconds;

	// Shift history Samples when it's time to record a new one.
	if (SamplingData.NumHistorySamples > 0 && FMath::Abs(Trajectory.Samples[SamplingData.NumHistorySamples - 1].AccumulatedSeconds) >= SamplingData.SecondsPerHistorySample)
	{
		for (int32 Index = 0; Index < SamplingData.NumHistorySamples - 1; ++Index)
		{
			Trajectory.Samples[Index].AccumulatedSeconds = Trajectory.Samples[Index + 1].AccumulatedSeconds;
			TranslationHistory[Index] = TranslationHistory[Index + 1];
			TranslationHistory[Index] += CurrentTranslation;
		}

		Trajectory.Samples[SamplingData.NumHistorySamples - 1].AccumulatedSeconds = 0.f;
		TranslationHistory[SamplingData.NumHistorySamples - 1] = CurrentTranslation;
	}
	else
	{
		for (int32 Index = 0; Index < SamplingData.NumHistorySamples; ++Index)
		{
			TranslationHistory[Index] += CurrentTranslation;
		}
	}

	// Update trajectory samples by applying the tracked translations to the current world position.
	FVector CurrentPositionWS = CharacterTrajectoryData.SkelMeshComponent->GetComponentLocation();
	for (int32 Index = 0; Index < SamplingData.NumHistorySamples; ++Index)
	{
		Trajectory.Samples[Index].AccumulatedSeconds -= DeltaSeconds;
		Trajectory.Samples[Index].Position = CurrentPositionWS - TranslationHistory[Index];

		// @todo: Handle facing. We currently don't use facing for history in any of our content. We will need the rotation intent of the character.
		Trajectory.Samples[Index].Facing = FQuat::Identity;
	}
}

FVector FMotionTrajectoryLibrary::RemapVectorMagnitudeWithCurve(const FVector& Vector, bool bUseCurve, const FRuntimeFloatCurve& Curve)
{
	if (bUseCurve)
	{
		const float Length = Vector.Length();
		if (Length > UE_KINDA_SMALL_NUMBER)
		{
			const float RemappedLength = Curve.GetRichCurveConst()->Eval(Length);
			return Vector * (RemappedLength / Length);
		}
	}

	return Vector;
}

void FMotionTrajectoryLibrary::UpdatePrediction_SimulateCharacterMovement(FPoseSearchQueryTrajectory& Trajectory,
	const FCharacterTrajectoryData& CharacterTrajectoryData, const FTrajectorySamplingData& SamplingData, float DeltaSeconds)
{
	if (!ensure(CharacterTrajectoryData.IsValid()))
	{
		return;
	}

	FVector CurrentPositionWS = CharacterTrajectoryData.SkelMeshComponent->GetComponentLocation();
	FVector CurrentVelocityWS = FMotionTrajectoryLibrary::RemapVectorMagnitudeWithCurve(CharacterTrajectoryData.CharacterMovementComponent->Velocity,
		CharacterTrajectoryData.bUseSpeedRemappingCurve, CharacterTrajectoryData.SpeedRemappingCurve);
	FVector CurrentAccelerationWS = FMotionTrajectoryLibrary::RemapVectorMagnitudeWithCurve(CharacterTrajectoryData.CharacterMovementComponent->GetCurrentAcceleration(),
		CharacterTrajectoryData.bUseAccelerationRemappingCurve, CharacterTrajectoryData.AccelerationRemappingCurve);
	FQuat CurrentFacingWS = CharacterTrajectoryData.SkelMeshComponent->GetComponentRotation().Quaternion();
	FQuat SkelMeshCompRelativeRotation = CharacterTrajectoryData.SkelMeshComponent->GetRelativeRotation().Quaternion();

	FQuat ControllerRotationPerStep = (CharacterTrajectoryData.ControllerRotationRate * SamplingData.SecondsPerPredictionSample).Quaternion();

	float AccumulatedSeconds = 0.f;

	const int32 LastIndex = Trajectory.Samples.Num() - 1;
	if (SamplingData.NumHistorySamples <= LastIndex)
	{
		for (int32 Index = SamplingData.NumHistorySamples; ; ++Index)
		{
			Trajectory.Samples[Index].Position = CurrentPositionWS;
			Trajectory.Samples[Index].Facing = CurrentFacingWS;
			Trajectory.Samples[Index].AccumulatedSeconds = AccumulatedSeconds;

			if (Index == LastIndex)
			{
				break;
			}

			CurrentPositionWS += CurrentVelocityWS * SamplingData.SecondsPerPredictionSample;
			AccumulatedSeconds += SamplingData.SecondsPerPredictionSample;

			// Account for the controller (e.g. the camera) rotating.
			CurrentFacingWS = ControllerRotationPerStep * CurrentFacingWS;
			CurrentAccelerationWS = FMotionTrajectoryLibrary::RemapVectorMagnitudeWithCurve(ControllerRotationPerStep * CurrentAccelerationWS,
				CharacterTrajectoryData.bUseAccelerationRemappingCurve, CharacterTrajectoryData.AccelerationRemappingCurve);

			FVector NewVelocityCS = FVector::ZeroVector;
			UCharacterMovementTrajectoryLibrary::StepCharacterMovementGroundPrediction(SamplingData.SecondsPerPredictionSample, CurrentVelocityWS, CurrentAccelerationWS, 
				CharacterTrajectoryData.CharacterMovementComponent, NewVelocityCS);
			CurrentVelocityWS = FMotionTrajectoryLibrary::RemapVectorMagnitudeWithCurve(NewVelocityCS,
				CharacterTrajectoryData.bUseSpeedRemappingCurve, CharacterTrajectoryData.SpeedRemappingCurve);

			if (CharacterTrajectoryData.CharacterMovementComponent->bOrientRotationToMovement && !CurrentAccelerationWS.IsNearlyZero())
			{
				// Rotate towards acceleration.
				const FVector CurrentAccelerationCS = SkelMeshCompRelativeRotation.RotateVector(CurrentAccelerationWS);
				CurrentFacingWS = FMath::QInterpConstantTo(CurrentFacingWS, CurrentAccelerationCS.ToOrientationQuat(), SamplingData.SecondsPerPredictionSample,
					CharacterTrajectoryData.RotateTowardsMovementSpeed);
			}
		}
	}
}