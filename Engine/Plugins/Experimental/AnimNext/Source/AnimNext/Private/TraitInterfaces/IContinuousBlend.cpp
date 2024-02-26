// Copyright Epic Games, Inc. All Rights Reserved.

#include "TraitInterfaces/IContinuousBlend.h"

#include "TraitCore/ExecutionContext.h"

namespace UE::AnimNext
{
	float IContinuousBlend::GetBlendWeight(const FExecutionContext& Context, const TTraitBinding<IContinuousBlend>& Binding, int32 ChildIndex) const
	{
		TTraitBinding<IContinuousBlend> SuperBinding;
		if (Binding.GetStackInterfaceSuper(SuperBinding))
		{
			return SuperBinding.GetBlendWeight(Context, ChildIndex);
		}

		return -1.0f;
	}
}
