// Copyright Epic Games, Inc. All Rights Reserved.

#include "Decorators/SequencePlayer.h"

#include "DecoratorBase/ExecutionContext.h"
#include "Param/ParamStack.h"
#include "Graph/AnimNext_LODPose.h"
#include "DecompressionTools.h"

namespace UE::AnimNext
{
	AUTO_REGISTER_ANIM_DECORATOR(FSequencePlayerDecorator)

	DEFINE_ANIM_DECORATOR_BEGIN(FSequencePlayerDecorator)
		DEFINE_ANIM_DECORATOR_IMPLEMENTS_INTERFACE(IEvaluate)
		DEFINE_ANIM_DECORATOR_IMPLEMENTS_INTERFACE(IUpdate)
		DEFINE_ANIM_DECORATOR_IMPLEMENTS_INTERFACE(ITimeline)
	DEFINE_ANIM_DECORATOR_END(FSequencePlayerDecorator)

	void FSequencePlayerDecorator::FInstanceData::Construct(FExecutionContext& Context, const FDecoratorBinding& Binding)
	{
		const FSharedData* SharedData = Binding.GetSharedData<FSharedData>();
		if (SharedData->AnimSequence != nullptr)
		{
			const float SequenceLength = SharedData->AnimSequence->GetPlayLength();
			InternalTimeAccumulator = FMath::Clamp(SharedData->GetStartPosition(Context, Binding), 0.0f, SequenceLength);
			PrevInternalTimeAccumulator = InternalTimeAccumulator;
		}
	}

	void FSequencePlayerDecorator::PreEvaluate(FExecutionContext& Context, const TDecoratorBinding<IEvaluate>& Binding) const
	{
		const FSharedData* SharedData = Binding.GetSharedData<FSharedData>();
		const FInstanceData* InstanceData = Binding.GetInstanceData<FInstanceData>();

		// TODO: Sample pose
		// FPose foo = ...
		// Context.PushPose(foo);

		FParamStack& ParamStack = FParamStack::Get();

		FDeltaTimeRecord DeltaTimeRecord;
		DeltaTimeRecord.Set(InstanceData->PrevInternalTimeAccumulator, InstanceData->InternalTimeAccumulator);

		const FAnimExtractContext ExtractionContext(static_cast<double>(InstanceData->InternalTimeAccumulator)
			, false /*Output.AnimInstanceProxy->ShouldExtractRootMotion()*/
			, DeltaTimeRecord
			, SharedData->GetbLoop(Context, Binding));

		const FAnimNextGraphReferencePose& GraphReferencePose = ParamStack.GetParam<FAnimNextGraphReferencePose>("GraphReferencePose");
		const int32 GraphLODLevel = ParamStack.GetParam<int32>("GraphLODLevel");
		const bool bGraphExpectsAdditive = ParamStack.GetParam<bool>("GraphExpectsAdditive");

		// HACK: Write to our output directly, only works if we have a single node in the graph that outputs a pose
		FAnimNextGraphLODPose& ResultPose = ParamStack.GetMutableParam<FAnimNextGraphLODPose>("ResultPose");

		ResultPose.LODPose.PrepareForLOD(*GraphReferencePose.ReferencePose, GraphLODLevel, true, bGraphExpectsAdditive);

		// Note : calling FDecompressionTools instead of UAnimSequence / UAnimSequenceBase, as I can not modify the engine code (so I extracted the function)
		// TODO : Revisit this, so we can plug other sequence types
		if (SharedData->AnimSequence != nullptr)
		{
			FDecompressionTools::GetAnimationPose(SharedData->AnimSequence, ResultPose.LODPose, ExtractionContext);
		}
	}

	double FSequencePlayerDecorator::GetPlayRate(FExecutionContext& Context, const TDecoratorBinding<ITimeline>& Binding) const
	{
		const FSharedData* SharedData = Binding.GetSharedData<FSharedData>();
		return SharedData->GetPlayRate(Context, Binding);
	}

	void FSequencePlayerDecorator::PreUpdate(FExecutionContext& Context, const TDecoratorBinding<IUpdate>& Binding) const
	{
		const FSharedData* SharedData = Binding.GetSharedData<FSharedData>();
		FInstanceData* InstanceData = Binding.GetInstanceData<FInstanceData>();

		if (SharedData->AnimSequence != nullptr)
		{
			FUpdateTraversalContext& TraversalContext = Context.GetTraversalContext<FUpdateTraversalContext>();

			TDecoratorBinding<ITimeline> TimelineDecorator;
			Context.GetInterface(Binding, TimelineDecorator);

			const float DeltaTime = TraversalContext.GetDeltaTime();
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
