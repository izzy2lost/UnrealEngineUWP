// Copyright Epic Games, Inc. All Rights Reserved.

#include "Decorators/SequencePlayer.h"

#include "AnimationRuntime.h"
#include "DecoratorBase/ExecutionContext.h"
#include "EvaluationVM/Tasks/PushAnimSequenceKeyframe.h"

namespace UE::AnimNext
{
	AUTO_REGISTER_ANIM_DECORATOR(FSequencePlayerDecorator)

	DEFINE_ANIM_DECORATOR_BEGIN(FSequencePlayerDecorator)
		DEFINE_ANIM_DECORATOR_IMPLEMENTS_INTERFACE(IEvaluate)
		DEFINE_ANIM_DECORATOR_IMPLEMENTS_INTERFACE(IUpdate)
		DEFINE_ANIM_DECORATOR_IMPLEMENTS_INTERFACE(ITimeline)
	DEFINE_ANIM_DECORATOR_END(FSequencePlayerDecorator)

	void FSequencePlayerDecorator::FInstanceData::Construct(const FExecutionContext& Context, const FDecoratorBinding& Binding)
	{
		const FSharedData* SharedData = Binding.GetSharedData<FSharedData>();
		if (SharedData->AnimSequence != nullptr)
		{
			const float SequenceLength = SharedData->AnimSequence->GetPlayLength();
			InternalTimeAccumulator = FMath::Clamp(SharedData->GetStartPosition(Context, Binding), 0.0f, SequenceLength);
		}
	}

	void FSequencePlayerDecorator::PreEvaluate(const FExecutionContext& Context, const TDecoratorBinding<IEvaluate>& Binding) const
	{
		const FSharedData* SharedData = Binding.GetSharedData<FSharedData>();

		const FInstanceData* InstanceData = Binding.GetInstanceData<FInstanceData>();
		const bool bInterpolate = true;

		FAnimNextAnimSequenceKeyframeTask Task = FAnimNextAnimSequenceKeyframeTask::MakeFromSampleTime(SharedData->AnimSequence, InstanceData->InternalTimeAccumulator, bInterpolate);
		Task.bExtractTrajectory = true;	/*Output.AnimInstanceProxy->ShouldExtractRootMotion()*/

		FEvaluateTraversalContext& TraversalContext = Context.GetTraversalContext<FEvaluateTraversalContext>();
		TraversalContext.AppendTask(Task);
	}

	float FSequencePlayerDecorator::GetPlayRate(const FExecutionContext& Context, const TDecoratorBinding<ITimeline>& Binding) const
	{
		const FSharedData* SharedData = Binding.GetSharedData<FSharedData>();
		return SharedData->GetPlayRate(Context, Binding);
	}

	float FSequencePlayerDecorator::AdvanceBy(const FExecutionContext& Context, const TDecoratorBinding<ITimeline>& Binding, float DeltaTime) const
	{
		const FSharedData* SharedData = Binding.GetSharedData<FSharedData>();
		if (UAnimSequence* AnimSeq = SharedData->AnimSequence.Get())
		{
			FInstanceData* InstanceData = Binding.GetInstanceData<FInstanceData>();

			TDecoratorBinding<ITimeline> TimelineDecorator;
			Context.GetInterface(Binding, TimelineDecorator);

			const float PlayRate = TimelineDecorator.GetPlayRate(Context);
			const bool bIsLooping = SharedData->GetbLoop(Context, Binding);
			const float SequenceLength = AnimSeq->GetPlayLength();

			FAnimationRuntime::AdvanceTime(bIsLooping, DeltaTime * PlayRate, InstanceData->InternalTimeAccumulator, SequenceLength);

			return FMath::Clamp(InstanceData->InternalTimeAccumulator / SequenceLength, 0.0f, 1.0f);
		}

		return 0.0f;
	}

	void FSequencePlayerDecorator::AdvanceToRatio(const FExecutionContext& Context, const TDecoratorBinding<ITimeline>& Binding, float ProgressRatio) const
	{
		const FSharedData* SharedData = Binding.GetSharedData<FSharedData>();
		if (UAnimSequence* AnimSeq = SharedData->AnimSequence.Get())
		{
			FInstanceData* InstanceData = Binding.GetInstanceData<FInstanceData>();

			const float SequenceLength = AnimSeq->GetPlayLength();

			InstanceData->InternalTimeAccumulator = FMath::Clamp(ProgressRatio, 0.0f, 1.0f) * SequenceLength;
		}
	}

	void FSequencePlayerDecorator::PreUpdate(FUpdateTraversalContext& Context, const TDecoratorBinding<IUpdate>& Binding, const FDecoratorUpdateState& DecoratorState) const
	{
		// We just advance the timeline
		TDecoratorBinding<ITimeline> TimelineDecorator;
		Context.GetInterface(Binding, TimelineDecorator);

		TimelineDecorator.AdvanceBy(Context, DecoratorState.GetDeltaTime());
	}
}
