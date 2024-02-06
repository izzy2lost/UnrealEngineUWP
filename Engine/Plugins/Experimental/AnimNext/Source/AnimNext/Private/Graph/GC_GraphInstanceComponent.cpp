// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/GC_GraphInstanceComponent.h"

#include "TraitCore/ExecutionContext.h"
#include "TraitInterfaces/IGarbageCollection.h"

namespace UE::AnimNext
{
	void FGCGraphInstanceComponent::Register(FAnimNextGraphInstance& GraphInstance, const FWeakTraitPtr& TraitPtr)
	{
		TraitsWithReferences.Add(FEntry(GraphInstance, TraitPtr));
	}

	void FGCGraphInstanceComponent::Unregister(const FWeakTraitPtr& TraitPtr)
	{
		const int32 EntryIndex = TraitsWithReferences.IndexOfByPredicate(
			[&TraitPtr](const FEntry& Entry)
			{
				return Entry.TraitPtr == TraitPtr;
			});

		if (ensure(EntryIndex != INDEX_NONE))
		{
			TraitsWithReferences.RemoveAtSwap(EntryIndex);
		}
	}

	void FGCGraphInstanceComponent::AddReferencedObjects(FReferenceCollector& Collector) const
	{
		FExecutionContext Context;
		TTraitBinding<IGarbageCollection> GCTrait;

		// TODO: If we kept the entries sorted by graph instance, we could re-use the execution context
		for (const FEntry& Entry : TraitsWithReferences)
		{
			Context.BindTo(Entry.GraphInstance);
			ensure(Context.GetInterface(Entry.TraitPtr, GCTrait));

			GCTrait.AddReferencedObjects(Context, Collector);
		}
	}
}
