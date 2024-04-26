// Copyright Epic Games, Inc. All Rights Reserved.

#include "TraitInterfaces/IDiscreteBlend.h"

#include "TraitCore/ExecutionContext.h"

namespace UE::AnimNext
{
	float IDiscreteBlend::GetBlendWeight(const FExecutionContext& Context, const TTraitBinding<IDiscreteBlend>& Binding, int32 ChildIndex) const
	{
		TTraitBinding<IDiscreteBlend> SuperBinding;
		if (Binding.GetStackInterfaceSuper(SuperBinding))
		{
			return SuperBinding.GetBlendWeight(Context, ChildIndex);
		}

		return -1.0f;
	}

	const FAlphaBlend* IDiscreteBlend::GetBlendState(const FExecutionContext& Context, const TTraitBinding<IDiscreteBlend>& Binding, int32 ChildIndex) const
	{
		TTraitBinding<IDiscreteBlend> SuperBinding;
		if (Binding.GetStackInterfaceSuper(SuperBinding))
		{
			return SuperBinding.GetBlendState(Context, ChildIndex);
		}

		return nullptr;
	}

	int32 IDiscreteBlend::GetBlendDestinationChildIndex(const FExecutionContext& Context, const TTraitBinding<IDiscreteBlend>& Binding) const
	{
		TTraitBinding<IDiscreteBlend> SuperBinding;
		if (Binding.GetStackInterfaceSuper(SuperBinding))
		{
			return SuperBinding.GetBlendDestinationChildIndex(Context);
		}

		return INDEX_NONE;
	}

	void IDiscreteBlend::OnBlendTransition(FExecutionContext& Context, const TTraitBinding<IDiscreteBlend>& Binding, int32 OldChildIndex, int32 NewChildIndex) const
	{
		TTraitBinding<IDiscreteBlend> SuperBinding;
		if (Binding.GetStackInterfaceSuper(SuperBinding))
		{
			return SuperBinding.OnBlendTransition(Context, OldChildIndex, NewChildIndex);
		}
	}

	void IDiscreteBlend::OnBlendInitiated(FExecutionContext& Context, const TTraitBinding<IDiscreteBlend>& Binding, int32 ChildIndex) const
	{
		TTraitBinding<IDiscreteBlend> SuperBinding;
		if (Binding.GetStackInterfaceSuper(SuperBinding))
		{
			return SuperBinding.OnBlendInitiated(Context, ChildIndex);
		}
	}

	void IDiscreteBlend::OnBlendTerminated(FExecutionContext& Context, const TTraitBinding<IDiscreteBlend>& Binding, int32 ChildIndex) const
	{
		TTraitBinding<IDiscreteBlend> SuperBinding;
		if (Binding.GetStackInterfaceSuper(SuperBinding))
		{
			return SuperBinding.OnBlendTerminated(Context, ChildIndex);
		}
	}
}
