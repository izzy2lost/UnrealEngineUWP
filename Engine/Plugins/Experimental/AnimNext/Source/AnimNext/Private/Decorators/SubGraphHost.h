// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "DecoratorBase/Decorator.h"
#include "DecoratorInterfaces/IDiscreteBlend.h"
#include "DecoratorInterfaces/IEvaluate.h"
#include "DecoratorInterfaces/IGarbageCollection.h"
#include "DecoratorInterfaces/IHierarchy.h"
#include "DecoratorInterfaces/IUpdate.h"
#include "Graph/AnimNextGraph.h"
#include "Graph/AnimNextGraphInstancePtr.h"

#include "SubGraphHost.generated.h"

USTRUCT(meta = (DisplayName = "SubGraph"))
struct FAnimNextSubGraphHostDecoratorSharedData : public FAnimNextDecoratorSharedData
{
	GENERATED_BODY()

	/** Subgraph that we host and manage. */
	UPROPERTY(EditAnywhere, Category = "Default")
	TObjectPtr<const UAnimNextGraph> SubGraph;

	// Latent pin support boilerplate
	#define DECORATOR_LATENT_PROPERTIES_ENUMERATOR(GeneratorMacro) \
		GeneratorMacro(SubGraph) \

	GENERATE_DECORATOR_LATENT_PROPERTIES(FAnimNextSubGraphHostDecoratorSharedData, DECORATOR_LATENT_PROPERTIES_ENUMERATOR)
	#undef DECORATOR_LATENT_PROPERTIES_ENUMERATOR
};

namespace UE::AnimNext
{
	/**
	 * FSubGraphHostDecorator
	 * 
	 * A decorator that hosts and manages a sub-graph instance.
	 */
	struct FSubGraphHostDecorator : FBaseDecorator, IEvaluate, IUpdate, IHierarchy, IDiscreteBlend, IGarbageCollection
	{
		DECLARE_ANIM_DECORATOR(FSubGraphHostDecorator, 0xb0cfe72e, FBaseDecorator)

		using FSharedData = FAnimNextSubGraphHostDecoratorSharedData;

		struct FInstanceData : FDecorator::FInstanceData
		{
			void Construct(const FExecutionContext& Context, const FDecoratorBinding& Binding);
			void Destruct(const FExecutionContext& Context, const FDecoratorBinding& Binding);

			struct FSubGraphSlot
			{
				// The sub-graph to use
				TObjectPtr<const UAnimNextGraph> SubGraph;

				// The graph instance
				FAnimNextGraphInstancePtr GraphInstance;
			};

			// List of sub-graph slots
			TArray<FSubGraphSlot> SubGraphSlots;

			// The index of the currently active sub-graph slot
			// All other sub-graphs are blending out
			int32 CurrentlyActiveSubGraphIndex = INDEX_NONE;
		};

		// IEvaluate impl
		virtual void PostEvaluate(FEvaluateTraversalContext& Context, const TDecoratorBinding<IEvaluate>& Binding) const override;

		// IUpdate impl
		virtual void PreUpdate(FUpdateTraversalContext& Context, const TDecoratorBinding<IUpdate>& Binding, const FDecoratorUpdateState& DecoratorState) const override;
		virtual void QueueChildrenForTraversal(FUpdateTraversalContext& Context, const TDecoratorBinding<IUpdate>& Binding, const FDecoratorUpdateState& DecoratorState, FUpdateTraversalQueue& TraversalQueue) const override;

		// IHierarchy impl
		virtual uint32 GetNumChildren(const FExecutionContext& Context, const TDecoratorBinding<IHierarchy>& Binding) const override;
		virtual void GetChildren(const FExecutionContext& Context, const TDecoratorBinding<IHierarchy>& Binding, FChildrenArray& Children) const override;

		// IDiscreteBlend impl
		virtual float GetBlendWeight(const FExecutionContext& Context, const TDecoratorBinding<IDiscreteBlend>& Binding, int32 ChildIndex) const override;
		virtual int32 GetBlendDestinationChildIndex(const FExecutionContext& Context, const TDecoratorBinding<IDiscreteBlend>& Binding) const override;
		virtual void OnBlendTransition(const FExecutionContext& Context, const TDecoratorBinding<IDiscreteBlend>& Binding, int32 OldChildIndex, int32 NewChildIndex) const override;
		virtual void OnBlendInitiated(const FExecutionContext& Context, const TDecoratorBinding<IDiscreteBlend>& Binding, int32 ChildIndex) const override;
		virtual void OnBlendTerminated(const FExecutionContext& Context, const TDecoratorBinding<IDiscreteBlend>& Binding, int32 ChildIndex) const override;

		// IGarbageCollection impl
		virtual void AddReferencedObjects(const FExecutionContext& Context, const TDecoratorBinding<IGarbageCollection>& Binding, FReferenceCollector& Collector) const override;
	};
}
