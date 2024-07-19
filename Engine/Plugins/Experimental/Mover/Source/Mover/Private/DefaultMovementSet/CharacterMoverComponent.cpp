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

bool UCharacterMoverComponent::IsCrouching() const
{
	return false; // Crouching not yet supported by Mover
}

bool UCharacterMoverComponent::IsFlying() const
{
	if (const UBaseMovementMode* Mode = GetMovementMode())
	{
		return Mode->GameplayTags.HasTag(Mover_IsFlying);
	}

	return false;
}

bool UCharacterMoverComponent::IsFalling() const
{
	if (const UBaseMovementMode* Mode = GetMovementMode())
	{
		return Mode->GameplayTags.HasTag(Mover_IsFalling);
	}

	return false;
}

bool UCharacterMoverComponent::IsAirborne() const
{
	if (const UBaseMovementMode* Mode = GetMovementMode())
	{
		return Mode->GameplayTags.HasTag(Mover_IsInAir);
	}

	return false;
}

bool UCharacterMoverComponent::IsOnGround() const
{
	if (const UBaseMovementMode* Mode = GetMovementMode())
	{
		return Mode->GameplayTags.HasTag(Mover_IsOnGround);
	}

	return false;
}

bool UCharacterMoverComponent::IsSwimming() const
{
	if (const UBaseMovementMode* Mode = GetMovementMode())
	{
		return Mode->GameplayTags.HasTag(Mover_IsSwimming);
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
