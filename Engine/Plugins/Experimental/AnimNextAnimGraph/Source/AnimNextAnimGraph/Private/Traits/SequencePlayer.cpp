// Copyright Epic Games, Inc. All Rights Reserved.

#include "Traits/SequencePlayer.h"

#include "AnimationRuntime.h"
#include "TraitCore/ExecutionContext.h"
#include "EvaluationVM/Tasks/PushAnimSequenceKeyframe.h"

namespace UE::AnimNext
{
	AUTO_REGISTER_ANIM_TRAIT(FSequencePlayerTrait)

	// Trait implementation boilerplate
	#define TRAIT_INTERFACE_ENUMERATOR(GeneratorMacro) \
		GeneratorMacro(IEvaluate) \
		GeneratorMacro(ITimeline) \
		GeneratorMacro(IUpdate) \

	GENERATE_ANIM_TRAIT_IMPLEMENTATION(FSequencePlayerTrait, TRAIT_INTERFACE_ENUMERATOR, NULL_ANIM_TRAIT_EVENT_ENUMERATOR)
	#undef TRAIT_INTERFACE_ENUMERATOR

	void FSequencePlayerTrait::FInstanceData::Construct(const FExecutionContext& Context, const FTraitBinding& Binding)
	{
		const FSharedData* SharedData = Binding.GetSharedData<FSharedData>();
		if (SharedData->AnimSequence != nullptr)
		{
			const float SequenceLength = SharedData->AnimSequence->GetPlayLength();
			InternalTimeAccumulator = FMath::Clamp(SharedData->GetStartPosition(Binding), 0.0f, SequenceLength);
		}
	}

	void FSequencePlayerTrait::PreEvaluate(FEvaluateTraversalContext& Context, const TTraitBinding<IEvaluate>& Binding) const
	{
		const FSharedData* SharedData = Binding.GetSharedData<FSharedData>();

		const FInstanceData* InstanceData = Binding.GetInstanceData<FInstanceData>();
		const bool bInterpolate = true;

		FAnimNextAnimSequenceKeyframeTask Task = FAnimNextAnimSequenceKeyframeTask::MakeFromSampleTime(SharedData->AnimSequence, InstanceData->InternalTimeAccumulator, bInterpolate);
		Task.bExtractTrajectory = true;	/*Output.AnimInstanceProxy->ShouldExtractRootMotion()*/

		Context.AppendTask(Task);
	}

	float FSequencePlayerTrait::GetPlayRate(const FExecutionContext& Context, const TTraitBinding<ITimeline>& Binding) const
	{
		const FSharedData* SharedData = Binding.GetSharedData<FSharedData>();
		return SharedData->GetPlayRate(Binding);
	}

	float FSequencePlayerTrait::AdvanceBy(const FExecutionContext& Context, const TTraitBinding<ITimeline>& Binding, float DeltaTime) const
	{
		const FSharedData* SharedData = Binding.GetSharedData<FSharedData>();
		if (UAnimSequence* AnimSeq = SharedData->AnimSequence.Get())
		{
			FInstanceData* InstanceData = Binding.GetInstanceData<FInstanceData>();

			TTraitBinding<ITimeline> TimelineTrait;
			Binding.GetStackInterface(TimelineTrait);

			const float PlayRate = TimelineTrait.GetPlayRate(Context);
			const bool bIsLooping = SharedData->GetbLoop(Binding);
			const float SequenceLength = AnimSeq->GetPlayLength();

			FAnimationRuntime::AdvanceTime(bIsLooping, DeltaTime * PlayRate, InstanceData->InternalTimeAccumulator, SequenceLength);

			return FMath::Clamp(InstanceData->InternalTimeAccumulator / SequenceLength, 0.0f, 1.0f);
		}

		return 0.0f;
	}

	void FSequencePlayerTrait::AdvanceToRatio(const FExecutionContext& Context, const TTraitBinding<ITimeline>& Binding, float ProgressRatio) const
	{
		const FSharedData* SharedData = Binding.GetSharedData<FSharedData>();
		if (UAnimSequence* AnimSeq = SharedData->AnimSequence.Get())
		{
			FInstanceData* InstanceData = Binding.GetInstanceData<FInstanceData>();

			const float SequenceLength = AnimSeq->GetPlayLength();

			InstanceData->InternalTimeAccumulator = FMath::Clamp(ProgressRatio, 0.0f, 1.0f) * SequenceLength;
		}
	}

	void FSequencePlayerTrait::PreUpdate(FUpdateTraversalContext& Context, const TTraitBinding<IUpdate>& Binding, const FTraitUpdateState& TraitState) const
	{
		// We just advance the timeline
		TTraitBinding<ITimeline> TimelineTrait;
		Binding.GetStackInterface(TimelineTrait);

		TimelineTrait.AdvanceBy(Context, TraitState.GetDeltaTime());
	}
}
