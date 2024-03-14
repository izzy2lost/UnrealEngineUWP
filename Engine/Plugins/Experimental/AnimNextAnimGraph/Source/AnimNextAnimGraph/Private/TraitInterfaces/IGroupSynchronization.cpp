// Copyright Epic Games, Inc. All Rights Reserved.

#include "TraitInterfaces/IGroupSynchronization.h"

#include "TraitCore/ExecutionContext.h"

namespace UE::AnimNext
{
	FName IGroupSynchronization::GetGroupName(const FExecutionContext& Context, const TTraitBinding<IGroupSynchronization>& Binding) const
	{
		TTraitBinding<IGroupSynchronization> SuperBinding;
		if (Binding.GetStackInterfaceSuper(SuperBinding))
		{
			return SuperBinding.GetGroupName(Context);
		}

		return NAME_None;
	}

	EAnimGroupRole::Type IGroupSynchronization::GetGroupRole(const FExecutionContext& Context, const TTraitBinding<IGroupSynchronization>& Binding) const
	{
		TTraitBinding<IGroupSynchronization> SuperBinding;
		if (Binding.GetStackInterfaceSuper(SuperBinding))
		{
			return SuperBinding.GetGroupRole(Context);
		}

		return EAnimGroupRole::CanBeLeader;
	}

	float IGroupSynchronization::AdvanceBy(const FExecutionContext& Context, const TTraitBinding<IGroupSynchronization>& Binding, float DeltaTime) const
	{
		TTraitBinding<IGroupSynchronization> SuperBinding;
		if (Binding.GetStackInterfaceSuper(SuperBinding))
		{
			return SuperBinding.AdvanceBy(Context, DeltaTime);
		}

		return 0.0f;
	}

	void IGroupSynchronization::AdvanceToRatio(const FExecutionContext& Context, const TTraitBinding<IGroupSynchronization>& Binding, float ProgressRatio) const
	{
		TTraitBinding<IGroupSynchronization> SuperBinding;
		if (Binding.GetStackInterfaceSuper(SuperBinding))
		{
			SuperBinding.AdvanceToRatio(Context, ProgressRatio);
		}
	}
}
