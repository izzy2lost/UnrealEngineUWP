// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DecoratorBase/IDecoratorInterface.h"
#include "DecoratorBase/ITraversalContext.h"
#include "EvaluationVM/EvaluationProgram.h"
#include "EvaluationVM/KeyframeState.h"

namespace UE::AnimNext
{
	/**
	 * FEvaluateTraversalContext
	 *
	 * Contains all relevant transient data for an evaluate traversal.
	 */
	struct ANIMNEXT_API FEvaluateTraversalContext : ITraversalContext
	{
		FEvaluateTraversalContext() = default;

		// Appends a new task into the evaluation program, tasks mutate state in the order they have been appended in
		// This means that child nodes need to evaluate first, tasks will usually be appended in IEvaluate::PostEvaluate
		// Tasks are moved into their final memory location, caller can allocate the task anywhere, it is no longer needed after this operation
		// @see FEvaluationProgram, FEvaluationTask, FEvaluationVM
		template<class TaskType>
		void AppendTask(TaskType&& Task) { EvaluationProgram->AppendTask(MoveTemp(Task)); }

	private:
		FEvaluationProgram* EvaluationProgram = nullptr;

		friend struct FScopedEvaluationProgram;
	};

	/**
	 * IEvaluate
	 * 
	 * This interface is called during the evaluation traversal. It aims to produce an evaluation program.
	 * 
	 * When a node is visited, PreEvaluate is first called on its top decorator. It is responsible for forwarding
	 * the call to the next decorator that implements this interface on the decorator stack of the node. Once
	 * all decorators have had the chance to PreEvaluate, the children of the decorator are queried through
	 * the IHierarchy interface. The children will then evaluate and PostEvaluate will then be called afterwards
	 * on the original decorator.
	 * 
	 * The execution context contains what to evaluate.
	 * @see FEvaluationProgram
	 */
	struct ANIMNEXT_API IEvaluate : IDecoratorInterface
	{
		DECLARE_ANIM_DECORATOR_INTERFACE(IEvaluate, 0xa303e9e7)

		// Called before a decorator's children are evaluated
		virtual void PreEvaluate(const FExecutionContext& Context, const TDecoratorBinding<IEvaluate>& Binding) const;

		// Called after a decorator's children have been evaluated
		virtual void PostEvaluate(const FExecutionContext& Context, const TDecoratorBinding<IEvaluate>& Binding) const;
	};

	/**
	 * Specialization for decorator binding.
	 */
	template<>
	struct TDecoratorBinding<IEvaluate> : FDecoratorBinding
	{
		// @see IEvaluate::PreEvaluate
		void PreEvaluate(const FExecutionContext& Context) const
		{
			GetInterface()->PreEvaluate(Context, *this);
		}

		// @see IEvaluate::PostEvaluate
		void PostEvaluate(const FExecutionContext& Context) const
		{
			GetInterface()->PostEvaluate(Context, *this);
		}

	protected:
		const IEvaluate* GetInterface() const { return GetInterfaceTyped<IEvaluate>(); }
	};

	/**
	 * Evaluates a sub-graph starting at its root and produces an evaluation program.
	 * Evaluation starts at the top of the stack that includes the graph root decorator.
	 *
	 * For each node:
	 *     - We call PreEvaluate on all its decorators
	 *     - We call GetChildren on all its decorators
	 *     - We evaluate all children found
	 *     - We call PostEvaluate on all its decorators
	 *
	 * @see IEvaluate::PreEvaluate, IEvaluate::PostEvaluate, IHierarchy::GetChildren
	 */
	[[nodiscard]] ANIMNEXT_API FEvaluationProgram EvaluateGraph(FExecutionContext& Context, FEvaluateTraversalContext& TraversalContext, FWeakDecoratorPtr GraphRootPtr);
}
