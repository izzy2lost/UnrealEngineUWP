// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TraitCore/TraitPtr.h"
#include "Graph/GraphInstanceComponent.h"

struct FAnimNextGraphInstance;
class FReferenceCollector;

namespace UE::AnimNext
{
	/**
	 * FGCGraphInstanceComponent
	 *
	 * This component maintains the necessary state to garbage collection.
	 */
	struct FGCGraphInstanceComponent : public FGraphInstanceComponent
	{
		DECLARE_ANIM_GRAPH_INSTANCE_COMPONENT(FGCGraphInstanceComponent)

		// Registers the provided trait with the GC system
		// Once registered, IGarbageCollection::AddReferencedObjects will be called on it during GC
		void Register(FAnimNextGraphInstance& GraphInstance, const FWeakTraitPtr& TraitPtr);

		// Unregisters the provided trait from the GC system
		void Unregister(const FWeakTraitPtr& TraitPtr);

		// Called during garbage collection to collect strong object references
		void AddReferencedObjects(FReferenceCollector& Collector) const;

	private:
		struct FEntry
		{
			FEntry(FAnimNextGraphInstance& InGraphInstance, const FWeakTraitPtr& InTraitPtr)
				: GraphInstance(InGraphInstance)
				, TraitPtr(InTraitPtr)
			{
			}

			FAnimNextGraphInstance& GraphInstance;
			FWeakTraitPtr TraitPtr;
		};

		// List of trait handles that contain UObject references and implement IGarbageCollection
		TArray<FEntry> TraitsWithReferences;
	};
}
