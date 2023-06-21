// Copyright Epic Games, Inc. All Rights Reserved.

#include "PoseSearch/AnimNode_BlendStackInput.h"
#include "PoseSearch/AnimNode_BlendStack.h"
#include "Animation/AnimInstanceProxy.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AnimNode_BlendStackInput)

void FAnimNode_BlendStackInput::Evaluate_AnyThread(FPoseContext& Output)
{
	check(Player);
	Player->Evaluate_AnyThread(Output);
}

void FAnimNode_BlendStackInput::Update_AnyThread(const FAnimationUpdateContext& Context)
{
	check(Player);
	Player->Update_AnyThread(Context);
}

