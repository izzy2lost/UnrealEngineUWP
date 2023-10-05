// Copyright Epic Games, Inc. All Rights Reserved.

#include "CharacterTrajectoryComponent.h"

#include "GameFramework/Character.h"
#include "HAL/IConsoleManager.h"
#include "MotionTrajectory.h"

#if ENABLE_ANIM_DEBUG
TAutoConsoleVariable<int32> CVarCharacterTrajectoryDebug(TEXT("a.CharacterTrajectory.Debug"), 0, TEXT("Turn on debug drawing for Character trajectory"));
#endif // ENABLE_ANIM_DEBUG

UCharacterTrajectoryComponent::UCharacterTrajectoryComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;

	bWantsInitializeComponent = true;
}

void UCharacterTrajectoryComponent::InitializeComponent()
{
	Super::InitializeComponent();

	if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		Character->OnCharacterMovementUpdated.AddDynamic(this, &UCharacterTrajectoryComponent::OnMovementUpdated);
	}
	else
	{
		UE_LOG(LogMotionTrajectory, Error, TEXT("UCharacterTrajectoryComponent requires its owner to be ACharacter"));
	}
}

void UCharacterTrajectoryComponent::UninitializeComponent()
{
	if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		Character->OnCharacterMovementUpdated.RemoveDynamic(this, &UCharacterTrajectoryComponent::OnMovementUpdated);
	}
	else
	{
		UE_LOG(LogMotionTrajectory, Error, TEXT("UCharacterTrajectoryComponent requires its owner to be ACharacter"));
	}

	Super::UninitializeComponent();
}

void UCharacterTrajectoryComponent::BeginPlay()
{
	Super::BeginPlay();

	SamplingData.Init();
	CharacterTrajectoryData.Init(GetOwner());

	FMotionTrajectoryLibrary::InitTrajectorySamples(Trajectory, CharacterTrajectoryData, SamplingData);
}

void UCharacterTrajectoryComponent::OnMovementUpdated(float DeltaSeconds, FVector OldLocation, FVector OldVelocity)
{
	if (LastUpdateFrameNumber != 0 && LastUpdateFrameNumber == GFrameNumber)
	{
		return;
	}

	if (DeltaSeconds <= 0.f)
	{
		return;
	}

	FMotionTrajectoryLibrary::UpdateHistory_ShiftInWorldSpace(Trajectory, SamplingData, DeltaSeconds);

	FMotionTrajectoryLibrary::UpdatePrediction_SimulateCharacterMovement(Trajectory, CharacterTrajectoryData, SamplingData, DeltaSeconds);

	LastUpdateFrameNumber = GFrameNumber;

#if ENABLE_ANIM_DEBUG
	if (CVarCharacterTrajectoryDebug.GetValueOnAnyThread())
	{
		Trajectory.DebugDrawTrajectory(GetWorld());
	}
#endif // ENABLE_ANIM_DEBUG
}