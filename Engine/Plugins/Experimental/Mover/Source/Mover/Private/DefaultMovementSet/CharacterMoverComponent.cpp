// Copyright Epic Games, Inc. All Rights Reserved.

#include "DefaultMovementSet/CharacterMoverComponent.h"

#include "DefaultMovementSet/InstantMovementEffects/BasicInstantMovementEffects.h"
#include "DefaultMovementSet/Modes/FallingMode.h"
#include "DefaultMovementSet/Modes/FlyingMode.h"
#include "DefaultMovementSet/Modes/WalkingMode.h"
#include "DefaultMovementSet/Settings/CommonLegacyMovementSettings.h"
#include "MoveLibrary/FloorQueryUtils.h"

UCharacterMoverComponent::UCharacterMoverComponent()
	: bHandleJump(true)
{
	// Default movement modes
	MovementModes.Add(DefaultModeNames::Walking, CreateDefaultSubobject<UWalkingMode>(TEXT("DefaultWalkingMode")));
	MovementModes.Add(DefaultModeNames::Falling, CreateDefaultSubobject<UFallingMode>(TEXT("DefaultFallingMode")));
	MovementModes.Add(DefaultModeNames::Flying,  CreateDefaultSubobject<UFlyingMode>(TEXT("DefaultFlyingMode")));

	StartingMovementMode = DefaultModeNames::Falling;
}

void UCharacterMoverComponent::BeginPlay()
{
	Super::BeginPlay();

	OnPreSimulationTick.AddDynamic(this, &UCharacterMoverComponent::OnMoverPreSimulationTick);
}

bool UCharacterMoverComponent::IsFalling() const
{
	if (bHasValidCachedState)
	{
		return CachedLastSyncState.MovementMode == DefaultModeNames::Falling;
	}

	return false;
}

bool UCharacterMoverComponent::IsAirborne() const
{
	if (bHasValidCachedState)
	{
		return CachedLastSyncState.MovementMode == DefaultModeNames::Flying || CachedLastSyncState.MovementMode == DefaultModeNames::Falling;
	}

	return false;
}

bool UCharacterMoverComponent::IsOnGround() const
{
	if (bHasValidCachedState)
	{
		return CachedLastSyncState.MovementMode == DefaultModeNames::Walking;
	}

	return false;
}

bool UCharacterMoverComponent::IsSwimming() const
{
	if (bHasValidCachedState)
	{
		return CachedLastSyncState.MovementMode == DefaultModeNames::Swimming;
	}

	return false;
}

bool UCharacterMoverComponent::IsSlopeSliding() const
{
	if (IsAirborne())
	{
		FFloorCheckResult HitResult;
		const UMoverBlackboard* MoverBlackboard = GetSimBlackboard();
		if (MoverBlackboard->TryGet(CommonBlackboard::LastFloorResult, HitResult))
		{
			return HitResult.bBlockingHit && !HitResult.bWalkableFloor;
		}
	}

	return false;
}

bool UCharacterMoverComponent::CanActorJump() const
{
	return IsOnGround();
}

bool UCharacterMoverComponent::Jump()
{
	if (const UCommonLegacyMovementSettings* CommonSettings = FindSharedSettings<UCommonLegacyMovementSettings>())
	{
		TSharedPtr<FJumpImpulseEffect> JumpMove = MakeShared<FJumpImpulseEffect>();
		JumpMove->UpwardsSpeed = CommonSettings->JumpUpwardsSpeed;
		
		QueueInstantMovementEffect(JumpMove);

		return true;
	}

	return false;
}

void UCharacterMoverComponent::OnMoverPreSimulationTick(const FMoverTimeStep& TimeStep, const FMoverInputCmdContext& InputCmd)
{
	if (bHandleJump)
	{
		const FCharacterDefaultInputs* CharacterInputs = InputCmd.InputCollection.FindDataByType<FCharacterDefaultInputs>();
	
		if (CharacterInputs && CharacterInputs->bIsJumpJustPressed && CanActorJump())
		{
			Jump();
		}
	}
}
