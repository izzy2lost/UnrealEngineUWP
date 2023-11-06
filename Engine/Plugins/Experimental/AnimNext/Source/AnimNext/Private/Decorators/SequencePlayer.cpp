// Copyright Epic Games, Inc. All Rights Reserved.

#include "Decorators/SequencePlayer.h"

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
			PrevInternalTimeAccumulator = InternalTimeAccumulator;
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

	double FSequencePlayerDecorator::GetPlayRate(const FExecutionContext& Context, const TDecoratorBinding<ITimeline>& Binding) const
	{
		const FSharedData* SharedData = Binding.GetSharedData<FSharedData>();
		return SharedData->GetPlayRate(Context, Binding);
	}

	void FSequencePlayerDecorator::PreUpdate(FUpdateTraversalContext& Context, const TDecoratorBinding<IUpdate>& Binding, const FDecoratorUpdateState& DecoratorState) const
	{
		const FSharedData* SharedData = Binding.GetSharedData<FSharedData>();
		if (SharedData->AnimSequence != nullptr)
		{
			FInstanceData* InstanceData = Binding.GetInstanceData<FInstanceData>();

			TDecoratorBinding<ITimeline> TimelineDecorator;
			Context.GetInterface(Binding, TimelineDecorator);

			const float DeltaTime = DecoratorState.GetDeltaTime();
			const float PlayRate = (float)TimelineDecorator.GetPlayRate(Context);

			const float EffectiveDelta = FMath::IsNearlyZero(DeltaTime) || FMath::IsNearlyZero(PlayRate) ? 0.f : DeltaTime * PlayRate;

			const bool bIsLooping = SharedData->GetbLoop(Context, Binding);

			const float SequenceLength = SharedData->AnimSequence->GetPlayLength();
			float CurrentTime = bIsLooping
				? FMath::Fmod(InstanceData->InternalTimeAccumulator + EffectiveDelta, SequenceLength)
				: FMath::Clamp(InstanceData->InternalTimeAccumulator + EffectiveDelta, 0.f, SequenceLength);

			if (bIsLooping && CurrentTime < 0.f)
			{
				CurrentTime += SequenceLength;
			}

			InstanceData->PrevInternalTimeAccumulator = InstanceData->InternalTimeAccumulator;
			InstanceData->InternalTimeAccumulator = CurrentTime;
		}
	}
}
