// Copyright Epic Games, Inc. All Rights Reserved.

#include "EvaluationVM/Tasks/PushAnimSequenceKeyframe.h"

#include "Animation/AnimSequence.h"
#include "BonePose.h"
#include "DecompressionTools.h"
#include "EvaluationVM/EvaluationVM.h"
#include "EvaluationVM/KeyframeState.h"

FAnimNextAnimSequenceKeyframeTask FAnimNextAnimSequenceKeyframeTask::MakeFromSampleTime(TWeakObjectPtr<UAnimSequence> AnimSequence, double SampleTime, bool bInterpolate)
{
	FAnimNextAnimSequenceKeyframeTask Task;
	Task.AnimSequence = AnimSequence;
	Task.SampleTime = SampleTime;
	Task.bInterpolate = bInterpolate;

	return Task;
}

FAnimNextAnimSequenceKeyframeTask FAnimNextAnimSequenceKeyframeTask::MakeFromKeyframeIndex(TWeakObjectPtr<UAnimSequence> AnimSequence, uint32 KeyframeIndex)
{
	FAnimNextAnimSequenceKeyframeTask Task;
	Task.AnimSequence = AnimSequence;
	Task.KeyframeIndex = KeyframeIndex;

	return Task;
}

void FAnimNextAnimSequenceKeyframeTask::Execute(UE::AnimNext::FEvaluationVM& VM) const
{
	using namespace UE::AnimNext;

	const UAnimSequence* AnimSequencePtr = AnimSequence.Get();
	const bool bIsAdditive = AnimSequencePtr->IsValidAdditive();

	FDeltaTimeRecord DeltaTimeRecord;	// Not needed
	const bool bExtractRootMotion = bExtractTrajectory;
	const bool bLooping = false;		// Not needed
	const bool bUseRawData = false;

	const FAnimExtractContext ExtractionContext(SampleTime, bExtractRootMotion, DeltaTimeRecord, bLooping);

	FKeyframeState Keyframe = VM.MakeUninitializedKeyframe(bIsAdditive);

	if (EnumHasAnyFlags(VM.GetFlags(), EEvaluationFlags::Bones))
	{
		FDecompressionTools::GetAnimationPose(AnimSequencePtr, Keyframe.Pose, ExtractionContext);
	}

	if (EnumHasAnyFlags(VM.GetFlags(), EEvaluationFlags::Curves))
	{
		AnimSequencePtr->EvaluateCurveData(Keyframe.Curves, static_cast<float>(SampleTime), bUseRawData);
	}

	if (EnumHasAnyFlags(VM.GetFlags(), EEvaluationFlags::Attributes))
	{
		FCompactPose Pose;		// Dummy but we need the bone container
		Pose.SetBoneContainer(&VM.GetBoneContainer());

		FAnimationPoseData PoseData(Pose, Keyframe.Curves, Keyframe.Attributes);

		AnimSequencePtr->EvaluateAttributes(PoseData, ExtractionContext, bUseRawData);
	}

	VM.PushValue(KEYFRAME_STACK_NAME, MakeUnique<FKeyframeState>(MoveTemp(Keyframe)));
}
