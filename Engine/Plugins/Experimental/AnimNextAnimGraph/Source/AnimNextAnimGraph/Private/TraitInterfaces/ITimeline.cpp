// Copyright Epic Games, Inc. All Rights Reserved.

#include "TraitInterfaces/ITimeline.h"

#include "TraitCore/ExecutionContext.h"

namespace UE::AnimNext
{
	float ITimeline::GetPlayRate(FExecutionContext& Context, const TTraitBinding<ITimeline>& Binding) const
	{
		TTraitBinding<ITimeline> SuperBinding;
		if (Binding.GetStackInterfaceSuper(SuperBinding))
		{
			return SuperBinding.GetPlayRate(Context);
		}

		return 1.0f;
	}

	FTimelineProgress ITimeline::GetProgress(FExecutionContext& Context, const TTraitBinding<ITimeline>& Binding) const
	{
		TTraitBinding<ITimeline> SuperBinding;
		if (Binding.GetStackInterfaceSuper(SuperBinding))
		{
			return SuperBinding.GetProgress(Context);
		}

		return FTimelineProgress();
	}

	FTimelineProgress ITimeline::SimulateAdvanceBy(FExecutionContext& Context, const TTraitBinding<ITimeline>& Binding, float DeltaTime) const
	{
		TTraitBinding<ITimeline> SuperBinding;
		if (Binding.GetStackInterfaceSuper(SuperBinding))
		{
			return SuperBinding.SimulateAdvanceBy(Context, DeltaTime);
		}

		return FTimelineProgress();
	}

	FTimelineProgress ITimeline::AdvanceBy(FExecutionContext& Context, const TTraitBinding<ITimeline>& Binding, float DeltaTime) const
	{
		TTraitBinding<ITimeline> SuperBinding;
		if (Binding.GetStackInterfaceSuper(SuperBinding))
		{
			return SuperBinding.AdvanceBy(Context, DeltaTime);
		}

		return FTimelineProgress();
	}

	void ITimeline::AdvanceToRatio(FExecutionContext& Context, const TTraitBinding<ITimeline>& Binding, float ProgressRatio) const
	{
		TTraitBinding<ITimeline> SuperBinding;
		if (Binding.GetStackInterfaceSuper(SuperBinding))
		{
			SuperBinding.AdvanceToRatio(Context, ProgressRatio);
		}
	}
}
