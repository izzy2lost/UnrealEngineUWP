// Copyright Epic Games, Inc. All Rights Reserved.

#include "TraitInterfaces/IGroupSynchronization.h"

#include "TraitCore/ExecutionContext.h"

namespace UE::AnimNext
{
	FName IGroupSynchronization::GetGroupName(FExecutionContext& Context, const TTraitBinding<IGroupSynchronization>& Binding) const
	{
		TTraitBinding<IGroupSynchronization> SuperBinding;
		if (Binding.GetStackInterfaceSuper(SuperBinding))
		{
			return SuperBinding.GetGroupName(Context);
		}

		return NAME_None;
	}

	EAnimGroupRole::Type IGroupSynchronization::GetGroupRole(FExecutionContext& Context, const TTraitBinding<IGroupSynchronization>& Binding) const
	{
		TTraitBinding<IGroupSynchronization> SuperBinding;
		if (Binding.GetStackInterfaceSuper(SuperBinding))
		{
			return SuperBinding.GetGroupRole(Context);
		}

		return EAnimGroupRole::CanBeLeader;
	}

	FTimelineProgress IGroupSynchronization::AdvanceBy(FExecutionContext& Context, const TTraitBinding<IGroupSynchronization>& Binding, float DeltaTime) const
	{
		TTraitBinding<IGroupSynchronization> SuperBinding;
		if (Binding.GetStackInterfaceSuper(SuperBinding))
		{
			return SuperBinding.AdvanceBy(Context, DeltaTime);
		}

		return FTimelineProgress();
	}

	void IGroupSynchronization::AdvanceToRatio(FExecutionContext& Context, const TTraitBinding<IGroupSynchronization>& Binding, float ProgressRatio) const
	{
		TTraitBinding<IGroupSynchronization> SuperBinding;
		if (Binding.GetStackInterfaceSuper(SuperBinding))
		{
			SuperBinding.AdvanceToRatio(Context, ProgressRatio);
		}
	}
}
