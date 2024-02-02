// Copyright Epic Games, Inc. All Rights Reserved.

#include "PhysicsMover/Transitions/PhysicsJumpCheck.h"
#include "Kinematic/LayeredMoves/BasicLayeredMoves.h"
#include "MoverComponent.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

UPhysicsJumpCheck::UPhysicsJumpCheck(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	TransitionToMode = KinematicModeNames::Falling;
}

FTransitionEvalResult UPhysicsJumpCheck::OnEvaluate(const FSimulationTickParams& Params) const
{
	FTransitionEvalResult EvalResult; 

	const FMoverTickStartData& StartState = Params.StartState;
	const FKinematicDefaultInputs* KinematicInputs = StartState.InputCmd.InputCollection.FindDataByType<FKinematicDefaultInputs>();
	check(KinematicInputs);

	if (KinematicInputs->bIsJumpJustPressed)
	{
		EvalResult.NextMode = TransitionToMode;
	}

	return EvalResult;
}

void UPhysicsJumpCheck::OnTrigger(const FSimulationTickParams& Params)
{
	TSharedPtr<FLayeredMove_JumpImpulse> JumpMove = MakeShared<FLayeredMove_JumpImpulse>();
	JumpMove->UpwardsSpeed = JumpUpwardsSpeed;

	Params.MoverComponent->QueueLayeredMove(JumpMove);

}

#if WITH_EDITOR
EDataValidationResult UPhysicsJumpCheck::IsDataValid(FDataValidationContext& Context) const
{
	return Super::IsDataValid(Context);
}
#endif // WITH_EDITOR
