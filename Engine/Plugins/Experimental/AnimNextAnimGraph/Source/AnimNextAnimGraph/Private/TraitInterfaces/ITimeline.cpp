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

	float ITimeline::AdvanceBy(FExecutionContext& Context, const TTraitBinding<ITimeline>& Binding, float DeltaTime) const
	{
		TTraitBinding<ITimeline> SuperBinding;
		if (Binding.GetStackInterfaceSuper(SuperBinding))
		{
			return SuperBinding.AdvanceBy(Context, DeltaTime);
		}

		return 0.0f;
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
