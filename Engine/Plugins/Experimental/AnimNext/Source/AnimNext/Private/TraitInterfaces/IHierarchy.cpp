// Copyright Epic Games, Inc. All Rights Reserved.

#include "TraitInterfaces/IHierarchy.h"

#include "TraitCore/ExecutionContext.h"

namespace UE::AnimNext
{
	uint32 IHierarchy::GetNumChildren(const FExecutionContext& Context, const TTraitBinding<IHierarchy>& Binding) const
	{
		TTraitBinding<IHierarchy> SuperBinding;
		if (Context.GetInterfaceSuper(Binding, SuperBinding))
		{
			return SuperBinding.GetNumChildren(Context);
		}

		return 0;
	}

	void IHierarchy::GetChildren(const FExecutionContext& Context, const TTraitBinding<IHierarchy>& Binding, FChildrenArray& Children) const
	{
		TTraitBinding<IHierarchy> SuperBinding;
		if (Context.GetInterfaceSuper(Binding, SuperBinding))
		{
			SuperBinding.GetChildren(Context, Children);
		}
	}
}
