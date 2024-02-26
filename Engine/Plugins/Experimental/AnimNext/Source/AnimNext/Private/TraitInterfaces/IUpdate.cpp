// Copyright Epic Games, Inc. All Rights Reserved.

#include "TraitInterfaces/IUpdate.h"

#include "TraitInterfaces/IHierarchy.h"
#include "Graph/GraphInstanceComponent.h"
#include "AnimNextStats.h"

DEFINE_STAT(STAT_AnimNext_UpdateGraph);

namespace UE::AnimNext
{
	namespace Private
	{
		// This structure is transient and lives either on the stack or the memstack and its destructor may not be called
		struct FUpdateEntry
		{
			// The trait state for this entry
			FTraitUpdateState			TraitState;

			// The trait handle that points to our node to update
			FWeakTraitPtr				TraitPtr;

			// Whether or not PreUpdate had been called already
			// TODO: Store bHasPreUpdated in the LSB of the entry pointer to save padding?
			bool						bHasPreUpdated = false;

			// The trait stack binding for this update entry
			FTraitStackBinding			TraitStack;

			// Once we've called PreUpdate, we cache the trait binding to avoid a redundant query to call PostUpdate
			TTraitBinding<IUpdate>		UpdateTrait;

			// These pointers are mutually exclusive
			// An entry is either part of the queued update stack, the update stack, the free list, or none of the above
			union
			{
				// Next entry in the stack of free entries
				FUpdateEntry* NextFreeEntry = nullptr;

				// Previous entry on the update stack
				FUpdateEntry* PrevUpdateStackEntry;

				// Previous entry on the queued update stack
				FUpdateEntry* PrevQueuedUpdateStackEntry;
			};

			FUpdateEntry(const FWeakTraitPtr& InTraitPtr, const FTraitUpdateState InTraitState)
				: TraitState(InTraitState)
				, TraitPtr(InTraitPtr)
			{
			}
		};
	}

	void IUpdate::PreUpdate(FUpdateTraversalContext& Context, const TTraitBinding<IUpdate>& Binding, const FTraitUpdateState& TraitState) const
	{
		TTraitBinding<IUpdate> SuperBinding;
		if (Binding.GetStackInterfaceSuper(SuperBinding))
		{
			SuperBinding.PreUpdate(Context, TraitState);
		}
	}

	void IUpdate::PostUpdate(FUpdateTraversalContext& Context, const TTraitBinding<IUpdate>& Binding, const FTraitUpdateState& TraitState) const
	{
		TTraitBinding<IUpdate> SuperBinding;
		if (Binding.GetStackInterfaceSuper(SuperBinding))
		{
			SuperBinding.PostUpdate(Context, TraitState);
		}
	}

	void IUpdateTraversal::QueueChildrenForTraversal(FUpdateTraversalContext& Context, const TTraitBinding<IUpdateTraversal>& Binding, const FTraitUpdateState& TraitState, FUpdateTraversalQueue& TraversalQueue) const
	{
		// Nothing to do
		// This function is called for each trait on the stack, one by one
		// No need to forward to our super
	}

	//////////////////////////////////////////////////////////////////////////
	// Traversal implementation

	void FUpdateTraversalContext::PushQueuedUpdateEntries(FUpdateTraversalQueue& TraversalQueue)
	{
		// Pop every entry from the queued update stack and push them onto the update stack
		// reversing their order
		while (Private::FUpdateEntry* Entry = TraversalQueue.QueuedUpdateStackHead)
		{
			// Update our queued stack head
			TraversalQueue.QueuedUpdateStackHead = Entry->PrevQueuedUpdateStackEntry;

			// Push our new entry onto the update stack
			PushUpdateEntry(Entry);
		}
	}

	void FUpdateTraversalContext::PushUpdateEntry(Private::FUpdateEntry* Entry)
	{
		Entry->PrevUpdateStackEntry = UpdateStackHead;
		UpdateStackHead = Entry;
	}

	Private::FUpdateEntry* FUpdateTraversalContext::PopUpdateEntry()
	{
		Private::FUpdateEntry* ChildEntry = UpdateStackHead;
		if (ChildEntry != nullptr)
		{
			// We have a child, set our new head
			UpdateStackHead = ChildEntry->PrevUpdateStackEntry;
		}

		return ChildEntry;
	}

	void FUpdateTraversalContext::PushFreeEntry(Private::FUpdateEntry* Entry)
	{
		Entry->NextFreeEntry = FreeEntryStackHead;
		FreeEntryStackHead = Entry;
	}

	Private::FUpdateEntry* FUpdateTraversalContext::GetNewEntry(const FWeakTraitPtr& TraitPtr, const FTraitUpdateState& TraitState)
	{
		Private::FUpdateEntry* FreeEntry = FreeEntryStackHead;
		if (FreeEntry != nullptr)
		{
			// We have a free entry, set our new head
			FreeEntryStackHead = FreeEntry->NextFreeEntry;

			// Update our entry
			FreeEntry->TraitState = TraitState;
			FreeEntry->TraitPtr = TraitPtr;
			FreeEntry->bHasPreUpdated = false;
			FreeEntry->NextFreeEntry = nullptr;		// Mark it as not being a member of any list
		}
		else
		{
			// Allocate a new entry
			FreeEntry = new(MemStack) Private::FUpdateEntry(TraitPtr, TraitState);
		}

		return FreeEntry;
	}

	FUpdateTraversalQueue::FUpdateTraversalQueue(FUpdateTraversalContext& InTraversalContext)
		: TraversalContext(InTraversalContext)
	{
	}

	void FUpdateTraversalQueue::Push(const FWeakTraitPtr& ChildPtr, const FTraitUpdateState& ChildTraitState)
	{
		if (!ChildPtr.IsValid())
		{
			return;	// Don't queue invalid pointers
		}

		Private::FUpdateEntry* ChildEntry = TraversalContext.GetNewEntry(ChildPtr, ChildTraitState);

		// We push children that are queued onto a stack
		// Once pre-update is done, we'll pop queued entries one by one and push them
		// onto the update stack
		// This has the effect of reversing the entries so that they are traversed in
		// the same order they are queued in:
		//    - First queued will be at the bottom of the queued stack and it ends up at the
		//      at the top of the update stack (last entry pushed)
		ChildEntry->PrevQueuedUpdateStackEntry = QueuedUpdateStackHead;
		QueuedUpdateStackHead = ChildEntry;
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
	// the trait bindings for IUpdate and IHierarchy (and re-use the binding for IUpdate for PostUpdate).
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

	void UpdateGraph(FAnimNextGraphInstancePtr& GraphInstance, float DeltaTime)
	{
		SCOPE_CYCLE_COUNTER(STAT_AnimNext_UpdateGraph);
		
		if (!GraphInstance.IsValid())
		{
			return;	// Nothing to update
		}

		if (!ensure(GraphInstance.IsRoot()))
		{
			return;	// We can only update starting at the root
		}

		FUpdateTraversalContext TraversalContext;

		FMemStack& MemStack = TraversalContext.GetMemStack();
		FMemMark Mark(MemStack);

		FChildrenArray Children;
		TTraitBinding<IHierarchy> HierarchyTrait;
		TTraitBinding<IUpdateTraversal> UpdateTraversalTrait;
		FTraitBinding TraitBinding;

		FUpdateTraversalQueue TraversalQueue(TraversalContext);

		// Before we start the traversal, we give the graph instance components the chance to do some work
		TraversalContext.BindTo(GraphInstance);
		for (auto It = TraversalContext.GetComponentIterator(); It; ++It)
		{
			It.Value()->PreUpdate(TraversalContext);
		}

		// Add the graph root to start the update process
		Private::FUpdateEntry RootEntry(GraphInstance.GetGraphRootPtr(), FTraitUpdateState(DeltaTime));
		TraversalContext.PushUpdateEntry(&RootEntry);

		while (Private::FUpdateEntry* Entry = TraversalContext.PopUpdateEntry())
		{
			const FWeakTraitPtr& EntryTraitPtr = Entry->TraitPtr;

			if (!Entry->bHasPreUpdated)
			{
				// This is the first time we visit this node, time to pre-update

				// Bind and cache our trait stack
				ensure(TraversalContext.GetStack(EntryTraitPtr, Entry->TraitStack));

				// First, if it has latent pins, we must execute and cache their results
				// This will ensure that other calls into this node will have a consistent view of
				// what the node saw when it started to update. We thus take a snapshot.
				const bool bIsFrozen = false;	// Not yet supported
				Entry->TraitStack.SnapshotLatentProperties(bIsFrozen);

				// If this trait stack implements IUpdate, call into it
				if (Entry->TraitStack.GetInterface(Entry->UpdateTrait))
				{
					Entry->UpdateTrait.PreUpdate(TraversalContext, Entry->TraitState);

					// Make sure that next time we visit this entry, we'll post-update
					Entry->bHasPreUpdated = true;

					// Push this entry onto the update stack, we'll call it once all our children have finished executing
					TraversalContext.PushUpdateEntry(Entry);
				}
				else
				{
					// We don't need this entry anymore
					TraversalContext.PushFreeEntry(Entry);
				}

				// Now visit the trait stack and queue our children
				ensure(Entry->TraitStack.GetTopTrait(TraitBinding));
				do
				{
					if (TraitBinding.AsInterface(UpdateTraversalTrait))
					{
						// Request that the trait queues the children it wants to visit
						// This is a separate function from PreUpdate to simplify traversal management. It is often the case that
						// the base trait is the one best placed to figure out how to optimally queue children since it
						// owns the handles to them. However, if an additive trait wishes to override PreUpdate, it might want
						// to perform logic after the base PreUpdate but before children are queued. Without a separate function,
						// we would have to rewrite the base PreUpdate entirely and use IHierarchy to query the handles of our children.
						UpdateTraversalTrait.QueueChildrenForTraversal(TraversalContext, Entry->TraitState, TraversalQueue);


						// Iterate over our queued children and push them onto the update stack
						// We do this to allow children to be queued in traversal order which is intuitive
						// but to traverse them in that order, they must be pushed in reverse order onto the update stack
						TraversalContext.PushQueuedUpdateEntries(TraversalQueue);
					}
					else if (TraitBinding.AsInterface(HierarchyTrait))
					{
						HierarchyTrait.GetChildren(TraversalContext, Children);

						// Append our children in reserve order so that they are visited in the same order they were added
						for (int32 ChildIndex = Children.Num() - 1; ChildIndex >= 0; --ChildIndex)
						{
							const FWeakTraitPtr& ChildPtr = Children[ChildIndex];
							if (ChildPtr.IsValid())
							{
								Private::FUpdateEntry* ChildEntry = TraversalContext.GetNewEntry(ChildPtr, Entry->TraitState);
								TraversalContext.PushUpdateEntry(ChildEntry);
							}
						}

						// Reset our container for the next time we need it
						Children.Reset();
					}
					else
					{
						// We don't have any children since we don't implement any of the relevant interfaces
					}
				} while (Entry->TraitStack.GetParentTrait(TraitBinding, TraitBinding));
			}
			else
			{
				// We've already visited this node once, time to PostUpdate
				check(Entry->UpdateTrait.IsValid());
				Entry->UpdateTrait.PostUpdate(TraversalContext, Entry->TraitState);

				// We don't need this entry anymore
				TraversalContext.PushFreeEntry(Entry);
			}
		}

		// After we finish the traversal, we give the graph instance components the chance to do some work
		TraversalContext.BindTo(GraphInstance);
		for (auto It = TraversalContext.GetComponentIterator(); It; ++It)
		{
			It.Value()->PostUpdate(TraversalContext);
		}
	}
}
