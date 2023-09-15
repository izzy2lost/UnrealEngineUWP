// Copyright Epic Games, Inc. All Rights Reserved.

#include "DecoratorInterfaces/IUpdate.h"

#include "DecoratorBase/ExecutionContext.h"
#include "DecoratorInterfaces/IHierarchy.h"

namespace UE::AnimNext
{
	FUpdateTraversalContext::FUpdateTraversalContext(float InDeltaTime)
		: DeltaTime(InDeltaTime)
	{
	}

	void IUpdate::PreUpdate(FExecutionContext& Context, const TDecoratorBinding<IUpdate>& Binding) const
	{
		TDecoratorBinding<IUpdate> SuperBinding;
		if (Context.GetInterfaceSuper(Binding, SuperBinding))
		{
			SuperBinding.PreUpdate(Context);
		}
	}

	void IUpdate::PostUpdate(FExecutionContext& Context, const TDecoratorBinding<IUpdate>& Binding) const
	{
		TDecoratorBinding<IUpdate> SuperBinding;
		if (Context.GetInterfaceSuper(Binding, SuperBinding))
		{
			SuperBinding.PostUpdate(Context);
		}
	}

	enum class EUpdateStep
	{
		PreUpdate,
		PostUpdate,
	};

	struct FUpdateEntry
	{
		FWeakDecoratorPtr	DecoratorPtr;
		EUpdateStep			DesiredStep = EUpdateStep::PreUpdate;
	};

	void UpdateGraph(FExecutionContext& Context, FWeakDecoratorPtr GraphRootPtr, float DeltaTime)
	{
		if (!GraphRootPtr.IsValid())
		{
			return;	// Nothing to update
		}

		FMemMark Mark(FMemStack::Get());

		TArray<FUpdateEntry, TMemStackAllocator<>> NodesPendingUpdate;
		NodesPendingUpdate.Reserve(64);

		FChildrenArray Children;
		Children.Reserve(64);

		FUpdateTraversalContext TraversalContext(DeltaTime);

		FScopedTraversalContext ScopedTraversalContext(Context, TraversalContext);

		// Add the graph root to kick start the update process
		NodesPendingUpdate.Push({ GraphRootPtr, EUpdateStep::PreUpdate });

		// Process every node twice: pre-update and post-update
		while (!NodesPendingUpdate.IsEmpty())
		{
			// Grab the top most entry
			const bool bAllowShrinking = false;	// Don't allow shrinking to avoid churn
			const FUpdateEntry Entry = NodesPendingUpdate.Pop(bAllowShrinking);

			if (Entry.DesiredStep == EUpdateStep::PreUpdate)
			{
				// This is the first time we visit this node, time to pre-update
				// Queue our node again so that post-update is called afterwards
				NodesPendingUpdate.Push({ Entry.DecoratorPtr, EUpdateStep::PostUpdate });

				// Performance note
				// When we process an animation graph for a frame, typically we'll update first before we evaluate
				// As a result of this, when we query for the update interface here, we will likely hit cold memory
				// which will cache miss (by touching the graph instance for the first time).
				// 
				// The processor will cache miss and continue to process as many instructions as it can before the
				// out-of-order execution window fills up. This is problematic here because a lot of the subsequent
				// instructions depend on the node instance and the interface it returns. The processor will be unable
				// to execute any of the instructions that follow in the current loop iteration. However, it might
				// be able to get started on the next node entry which is likely to cache miss as well. Should the
				// processor make it that far and it turns out that we have to push a child onto the stack, all
				// of the work it tried to do ahead of time will have to be thrown away.
				// 
				// There are two things that we can do here to try and help performance: prefetch ahead and bulk
				// query.
				// 
				// If we prefetch, we have to be careful because we do not know what the node will do in its PreUpdate.
				// If it turns out that it does a lot of work, our prefetch might end up getting thrown out. This is
				// because prefetched cache lines typically end up being the first evicted unless they are touched first.
				// It is thus dangerous to use manual prefetching when the memory access pattern isn't fully known.
				// In practice, it is likely viable as most nodes won't do too much work.
				// 
				// A better approach could be to instead bulk query for our interfaces. We could cache in the FUpdateEntry
				// the decorator bindings for IUpdate and IHierarchy (and re-use the binding for IUpdate for PostUpdate).
				// Every iteration we could check how many children are queued up on the stack. We could then grab N
				// entries (2 to 4) and query their interfaces in bulk. The idea is to clump the cache miss instructions
				// together and to interleave the interface queries. This will queue up as much work as possible in the
				// out-of-order execution window that will not be thrown away because of a branch. Eventually the first
				// interface query will complete and execution will resume here to call PreUpdate etc. This will be able
				// to happen while the processor still waits on the cache misses and finishes the interface query of the
				// other bulked children. The same effect could be achieved by querying the interfaces after the call
				// to GetChildren by bulk querying all of them right then. This way, as soon as the execution window
				// can clear the end of the loop, it can start working on the next entry which will be warm in the L1
				// cache allowing the CPU to carry ahead before all child interfaces are fully resolved.
				// 
				// The above may seem like a stretch and an insignificant over optimization but it could very well be the
				// key to unlocking large performance gains during traversal. The above optimization would allow us to
				// perform as much useful work as possible while waiting for memory, hiding its slow latency by fully
				// leveraging out-of-order CPU execution.

				TDecoratorBinding<IUpdate> UpdateDecorator;
				if (Context.GetInterface(Entry.DecoratorPtr, UpdateDecorator))
				{
					UpdateDecorator.PreUpdate(Context);
				}

				TDecoratorBinding<IHierarchy> HierarchyDecorator;
				if (Context.GetInterface(Entry.DecoratorPtr, HierarchyDecorator))
				{
					HierarchyDecorator.GetChildren(Context, Children);

					// Append our children in reserve order so that they are visited in the same order they were added
					for (int32 ChildIndex = Children.Num() - 1; ChildIndex >= 0; --ChildIndex)
					{
						NodesPendingUpdate.Push({ Children[ChildIndex], EUpdateStep::PreUpdate });
					}

					// Reset our container for the next time we need it
					Children.Reset();
				}

				// Break and continue to the next top-most entry
				// It is either a child ready for its pre-update or our current entry ready for its post-update (leaf)
			}
			else
			{
				// We've already visited this node once, time to post-update
				TDecoratorBinding<IUpdate> UpdateDecorator;
				if (Context.GetInterface(Entry.DecoratorPtr, UpdateDecorator))
				{
					UpdateDecorator.PostUpdate(Context);
				}

				// Break and continue to the next top-most entry
				// It is either a sibling read for its post-update or our parent entry ready for its post-update
			}
		}
	}
}
