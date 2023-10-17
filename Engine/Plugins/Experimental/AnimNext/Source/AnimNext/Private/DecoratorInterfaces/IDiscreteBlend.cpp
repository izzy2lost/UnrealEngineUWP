// Copyright Epic Games, Inc. All Rights Reserved.

#include "DecoratorInterfaces/IDiscreteBlend.h"

#include "DecoratorBase/ExecutionContext.h"

namespace UE::AnimNext
{
	float IDiscreteBlend::GetBlendWeight(FExecutionContext& Context, const TDecoratorBinding<IDiscreteBlend>& Binding, int32 ChildIndex) const
	{
		TDecoratorBinding<IDiscreteBlend> SuperBinding;
		if (Context.GetInterfaceSuper(Binding, SuperBinding))
		{
			return SuperBinding.GetBlendWeight(Context, ChildIndex);
		}

		return -1.0f;
	}

	const FAlphaBlend* IDiscreteBlend::GetBlendState(FExecutionContext& Context, const TDecoratorBinding<IDiscreteBlend>& Binding, int32 ChildIndex) const
	{
		TDecoratorBinding<IDiscreteBlend> SuperBinding;
		if (Context.GetInterfaceSuper(Binding, SuperBinding))
		{
			return SuperBinding.GetBlendState(Context, ChildIndex);
		}

		return nullptr;
	}

	int32 IDiscreteBlend::GetBlendDestinationChildIndex(FExecutionContext& Context, const TDecoratorBinding<IDiscreteBlend>& Binding) const
	{
		TDecoratorBinding<IDiscreteBlend> SuperBinding;
		if (Context.GetInterfaceSuper(Binding, SuperBinding))
		{
			return SuperBinding.GetBlendDestinationChildIndex(Context);
		}

		return INDEX_NONE;
	}

	void IDiscreteBlend::OnBlendTransition(FExecutionContext& Context, const TDecoratorBinding<IDiscreteBlend>& Binding, int32 OldChildIndex, int32 NewChildIndex) const
	{
		TDecoratorBinding<IDiscreteBlend> SuperBinding;
		if (Context.GetInterfaceSuper(Binding, SuperBinding))
		{
			return SuperBinding.OnBlendTransition(Context, OldChildIndex, NewChildIndex);
		}
	}

	void IDiscreteBlend::OnBlendInitiated(FExecutionContext& Context, const TDecoratorBinding<IDiscreteBlend>& Binding, int32 ChildIndex) const
	{
		TDecoratorBinding<IDiscreteBlend> SuperBinding;
		if (Context.GetInterfaceSuper(Binding, SuperBinding))
		{
			return SuperBinding.OnBlendInitiated(Context, ChildIndex);
		}
	}

	void IDiscreteBlend::OnBlendTerminated(FExecutionContext& Context, const TDecoratorBinding<IDiscreteBlend>& Binding, int32 ChildIndex) const
	{
		TDecoratorBinding<IDiscreteBlend> SuperBinding;
		if (Context.GetInterfaceSuper(Binding, SuperBinding))
		{
			return SuperBinding.OnBlendTerminated(Context, ChildIndex);
		}
	}
}
