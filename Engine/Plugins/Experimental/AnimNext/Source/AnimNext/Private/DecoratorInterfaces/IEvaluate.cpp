// Copyright Epic Games, Inc. All Rights Reserved.

#include "DecoratorInterfaces/IEvaluate.h"

#include "DecoratorBase/ExecutionContext.h"
#include "DecoratorInterfaces/IHierarchy.h"
#include "AnimNextStats.h"

DEFINE_STAT(STAT_AnimNext_EvaluateGraph);

namespace UE::AnimNext
{
	/**
	 * FScopedEvaluationProgram
	 *
	 * Pushes and pops a evaluation program onto the evaluation traversal context.
	 * During the lifetime of an instance of this object, the traversal context will
	 * return the provided evaluation program as the current evaluation program.
	 *
	 * @see FEvaluateTraversalContext
	 */
	struct ANIMNEXT_API FScopedEvaluationProgram final
	{
		FScopedEvaluationProgram(FEvaluateTraversalContext& InTraversalContext, FEvaluationProgram& InEvaluationProgram)
			: TraversalContext(InTraversalContext)
			, OldEvaluationProgram(InTraversalContext.EvaluationProgram)
		{
			InTraversalContext.EvaluationProgram = &InEvaluationProgram;
		}

		~FScopedEvaluationProgram()
		{
			TraversalContext.EvaluationProgram = OldEvaluationProgram;
		}

		FScopedEvaluationProgram(const FScopedEvaluationProgram&) = delete;
		FScopedEvaluationProgram& operator=(const FScopedEvaluationProgram&) = delete;

	private:
		FEvaluateTraversalContext& TraversalContext;
		FEvaluationProgram* OldEvaluationProgram;
	};

	void IEvaluate::PreEvaluate(FExecutionContext& Context, const TDecoratorBinding<IEvaluate>& Binding) const
	{
		TDecoratorBinding<IEvaluate> SuperBinding;
		if (Context.GetInterfaceSuper(Binding, SuperBinding))
		{
			SuperBinding.PreEvaluate(Context);
		}
	}

	void IEvaluate::PostEvaluate(FExecutionContext& Context, const TDecoratorBinding<IEvaluate>& Binding) const
	{
		TDecoratorBinding<IEvaluate> SuperBinding;
		if (Context.GetInterfaceSuper(Binding, SuperBinding))
		{
			SuperBinding.PostEvaluate(Context);
		}
	}

	enum class EEvaluateStep
	{
		PreEvaluate,
		PostEvaluate,
	};

	struct FEvaluateEntry
	{
		FWeakDecoratorPtr	DecoratorPtr;
		EEvaluateStep		DesiredStep = EEvaluateStep::PreEvaluate;
	};

	FEvaluationProgram EvaluateGraph(FExecutionContext& Context, FEvaluateTraversalContext& TraversalContext, FWeakDecoratorPtr GraphRootPtr)
	{
		SCOPE_CYCLE_COUNTER(STAT_AnimNext_EvaluateGraph);
		
		FEvaluationProgram EvaluationProgram;

		if (!GraphRootPtr.IsValid())
		{
			return EvaluationProgram;	// Nothing to update
		}

		FMemMark Mark(FMemStack::Get());

		TArray<FEvaluateEntry, TMemStackAllocator<>> NodesPendingUpdate;
		NodesPendingUpdate.Reserve(64);

		FChildrenArray Children;
		Children.Reserve(64);

		FScopedEvaluationProgram ScopedEvaluationProgram(TraversalContext, EvaluationProgram);
		FScopedTraversalContext ScopedTraversalContext(Context, TraversalContext);

		// Add the graph root to kick start the evaluation process
		NodesPendingUpdate.Push({ GraphRootPtr, EEvaluateStep::PreEvaluate });

		// Process every node twice: pre-evaluate and post-evaluate
		while (!NodesPendingUpdate.IsEmpty())
		{
			// Grab the top most entry
			const bool bAllowShrinking = false;	// Don't allow shrinking to avoid churn
			const FEvaluateEntry Entry = NodesPendingUpdate.Pop(bAllowShrinking);

			if (Entry.DesiredStep == EEvaluateStep::PreEvaluate)
			{
				// This is the first time we visit this node, time to pre-evaluate
				// Queue our node again so that post-evaluate is called afterwards
				NodesPendingUpdate.Push({ Entry.DecoratorPtr, EEvaluateStep::PostEvaluate });

				TDecoratorBinding<IEvaluate> EvaluateDecorator;
				if (Context.GetInterface(Entry.DecoratorPtr, EvaluateDecorator))
				{
					EvaluateDecorator.PreEvaluate(Context);
				}

				TDecoratorBinding<IHierarchy> HierarchyDecorator;
				if (Context.GetInterface(Entry.DecoratorPtr, HierarchyDecorator))
				{
					HierarchyDecorator.GetChildren(Context, Children);

					// Append our children in reserve order so that they are visited in the same order they were added
					for (int32 ChildIndex = Children.Num() - 1; ChildIndex >= 0; --ChildIndex)
					{
						NodesPendingUpdate.Push({ Children[ChildIndex], EEvaluateStep::PreEvaluate });
					}

					// Reset our container for the next time we need it
					Children.Reset();
				}

				// Break and continue to the next top-most entry
				// It is either a child ready for its pre-evaluate or our current entry ready for its post-evaluate (leaf)
			}
			else
			{
				// We've already visited this node once, time to post-evaluate
				TDecoratorBinding<IEvaluate> EvaluateDecorator;
				if (Context.GetInterface(Entry.DecoratorPtr, EvaluateDecorator))
				{
					EvaluateDecorator.PostEvaluate(Context);
				}

				// Break and continue to the next top-most entry
				// It is either a sibling read for its post-evaluate or our parent entry ready for its post-evaluate
			}
		}

		return EvaluationProgram;
	}
}
