// Copyright Epic Games, Inc. All Rights Reserved.

#include "DecoratorInterfaces/IUpdate.h"

#include "DecoratorBase/ExecutionContext.h"
#include "DecoratorInterfaces/IHierarchy.h"
#include "AnimNextStats.h"

DEFINE_STAT(STAT_AnimNext_UpdateGraph);

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

	// This structure is transient and lives either on the stack or the memstack and its destructor may not be called
	struct FUpdateEntry
	{
		// The decorator handle that points to our node to update
		FWeakDecoratorPtr	DecoratorPtr;

		// Which step we wish to perform when we next see this entry
		EUpdateStep			DesiredStep = EUpdateStep::PreUpdate;

		// Once we've called PreUpdate, we cache the decorator binding to avoid a redundant query to call PostUpdate
		TDecoratorBinding<IUpdate> UpdateDecorator;

		// These pointers are mutually exclusive
		// An entry is either part of the pending stack, the free list, or neither
		union
		{
			// Next entry in the linked list of free entries
			FUpdateEntry* NextFreeEntry = nullptr;

			// Previous entry below us in the nodes pending stack
			FUpdateEntry* PrevStackEntry;
		};

		FUpdateEntry(const FWeakDecoratorPtr& InDecoratorPtr, EUpdateStep InDesiredStep, FUpdateEntry* InPrevStackEntry = nullptr)
			: DecoratorPtr(InDecoratorPtr)
			, DesiredStep(InDesiredStep)
			, PrevStackEntry(InPrevStackEntry)
		{
		}
	};

	void UpdateGraph(FExecutionContext& Context, FUpdateTraversalContext& TraversalContext, FWeakDecoratorPtr GraphRootPtr)
	{
		SCOPE_CYCLE_COUNTER(STAT_AnimNext_UpdateGraph);
		
		if (!GraphRootPtr.IsValid())
		{
			return;	// Nothing to update
		}

		FMemStack& MemStack = FMemStack::Get();
		FMemMark Mark(MemStack);

		FChildrenArray Children;
		Children.Reserve(16);

		FScopedTraversalContext ScopedTraversalContext(Context, TraversalContext);

		// Add the graph root to kick start the update process
		FUpdateEntry GraphRootEntry(GraphRootPtr, EUpdateStep::PreUpdate);
		FUpdateEntry* NodesPendingUpdateStackTop = &GraphRootEntry;

		// List of free entries we can recycle
		FUpdateEntry* FreeEntryList = nullptr;

		TDecoratorBinding<IHierarchy> HierarchyDecorator;

		// Process every node twice: pre-update and post-update
		while (NodesPendingUpdateStackTop != nullptr)
		{
			// Grab the top most entry
			FUpdateEntry* Entry = NodesPendingUpdateStackTop;
			bool bIsEntryUsed = true;

			if (Entry->DesiredStep == EUpdateStep::PreUpdate)
			{
				if (Context.GetInterface(Entry->DecoratorPtr, Entry->UpdateDecorator))
				{
					// This is the first time we visit this node, time to pre-update
					Entry->UpdateDecorator.PreUpdate(Context);

					// Leave our entry on top of the stack, we'll need to call PostUpdate once the children
					// we'll push on top finish
					Entry->DesiredStep = EUpdateStep::PostUpdate;
				}
				else
				{
					// This node doesn't implement IUpdate, we can pop it from the stack
					NodesPendingUpdateStackTop = Entry->PrevStackEntry;
					bIsEntryUsed = false;
				}

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

				if (Context.GetInterface(Entry->DecoratorPtr, HierarchyDecorator))
				{
					HierarchyDecorator.GetChildren(Context, Children);

					// Append our children in reserve order so that they are visited in the same order they were added
					for (int32 ChildIndex = Children.Num() - 1; ChildIndex >= 0; --ChildIndex)
					{
						// Insert our new child on top of the stack

						FUpdateEntry* ChildEntry;
						if (FreeEntryList != nullptr)
						{
							// Grab an entry from the free list
							ChildEntry = FreeEntryList;
							FreeEntryList = ChildEntry->NextFreeEntry;

							ChildEntry->DecoratorPtr = Children[ChildIndex];
							ChildEntry->DesiredStep = EUpdateStep::PreUpdate;
							ChildEntry->PrevStackEntry = NodesPendingUpdateStackTop;
						}
						else
						{
							// Allocate a new entry
							ChildEntry = new(MemStack) FUpdateEntry(Children[ChildIndex], EUpdateStep::PreUpdate, NodesPendingUpdateStackTop);
						}

						NodesPendingUpdateStackTop = ChildEntry;
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
				check(Entry->UpdateDecorator.IsValid());
				Entry->UpdateDecorator.PostUpdate(Context);

				// Now that we are done processing this entry, we can pop it
				NodesPendingUpdateStackTop = Entry->PrevStackEntry;
				bIsEntryUsed = false;

				// Break and continue to the next top-most entry
				// It is either a sibling read for its post-update or our parent entry ready for its post-update
			}

			if (!bIsEntryUsed)
			{
				// This entry is no longer used, add it to the free list
				Entry->NextFreeEntry = FreeEntryList;
				FreeEntryList = Entry;
			}
		}
	}
}
