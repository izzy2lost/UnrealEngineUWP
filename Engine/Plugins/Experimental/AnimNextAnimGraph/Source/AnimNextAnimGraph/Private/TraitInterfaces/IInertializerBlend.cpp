// Copyright Epic Games, Inc. All Rights Reserved.

#include "TraitInterfaces/IInertializerBlend.h"

#include "TraitCore/ExecutionContext.h"

namespace UE::AnimNext
{
	float IInertializerBlend::GetBlendTime(FExecutionContext& Context, const TTraitBinding<IInertializerBlend>& Binding, int32 ChildIndex) const
	{
		TTraitBinding<IInertializerBlend> SuperBinding;
		if (Binding.GetStackInterfaceSuper(SuperBinding))
		{
			return SuperBinding.GetBlendTime(Context, ChildIndex);
		}

		return 0.0f;
	}
}
