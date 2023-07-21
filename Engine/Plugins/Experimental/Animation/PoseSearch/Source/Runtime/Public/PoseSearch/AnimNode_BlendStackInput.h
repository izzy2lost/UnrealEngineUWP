// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Animation/AnimNodeBase.h"
#include "AnimNode_BlendStackInput.generated.h"

/** Input pose that links the blend stack's sample graph with the sample/pose chosen by the blend stack.
*	@Todo: It might be better to reuse FAnimNode_LinkedInputPose, since we will most likely need variable input pins in the future too.
*/
USTRUCT(BlueprintInternalUseOnly, Experimental)
struct POSESEARCH_API FAnimNode_BlendStackInput : public FAnimNode_Base
{
	GENERATED_BODY()
	
public:

	virtual void Update_AnyThread(const FAnimationUpdateContext& Context) override;
	virtual void Evaluate_AnyThread(FPoseContext& Output) override;

	UPROPERTY()
	int32 SampleIndex = INDEX_NONE;

	UPROPERTY()
	int32 BlendStackAllocationIndex = INDEX_NONE;

	// The player is guaranteed to be valid for the whole duration of update/eval.
	struct FPoseSearchAnimPlayer** Player;
};

