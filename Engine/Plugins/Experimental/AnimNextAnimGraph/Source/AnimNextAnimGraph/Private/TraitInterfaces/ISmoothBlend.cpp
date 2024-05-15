// Copyright Epic Games, Inc. All Rights Reserved.

#include "TraitInterfaces/ISmoothBlend.h"

#include "TraitCore/ExecutionContext.h"

namespace UE::AnimNext
{
	float ISmoothBlend::GetBlendTime(FExecutionContext& Context, const TTraitBinding<ISmoothBlend>& Binding, int32 ChildIndex) const
	{
		TTraitBinding<ISmoothBlend> SuperBinding;
		if (Binding.GetStackInterfaceSuper(SuperBinding))
		{
			return SuperBinding.GetBlendTime(Context, ChildIndex);
		}

		return 0.0f;
	}
}
