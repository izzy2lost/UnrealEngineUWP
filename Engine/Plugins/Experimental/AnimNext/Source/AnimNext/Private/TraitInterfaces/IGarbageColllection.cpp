// Copyright Epic Games, Inc. All Rights Reserved.

#include "TraitInterfaces/IGarbageCollection.h"

#include "TraitCore/ExecutionContext.h"
#include "Graph/GC_GraphInstanceComponent.h"

namespace UE::AnimNext
{
	void IGarbageCollection::RegisterWithGC(const FExecutionContext& Context, const FTraitBinding& Binding)
	{
		FGCGraphInstanceComponent& Component = Context.GetComponent<FGCGraphInstanceComponent>();
		Component.Register(Binding.GetTraitPtr());
	}

	void IGarbageCollection::UnregisterWithGC(const FExecutionContext& Context, const FTraitBinding& Binding)
	{
		FGCGraphInstanceComponent& Component = Context.GetComponent<FGCGraphInstanceComponent>();
		Component.Unregister(Binding.GetTraitPtr());
	}

	void IGarbageCollection::AddReferencedObjects(const FExecutionContext& Context, const TTraitBinding<IGarbageCollection>& Binding, FReferenceCollector& Collector) const
	{
		TTraitBinding<IGarbageCollection> SuperBinding;
		if (Binding.GetStackInterfaceSuper(SuperBinding))
		{
			SuperBinding.AddReferencedObjects(Context, Collector);
		}
	}
}
