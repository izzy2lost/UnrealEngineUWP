// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TraitCore/ExecutionContext.h"

namespace UE::AnimNext
{
	/**
	 * FExecutionContextProxy
	 *
	 * Wraps an execution context and forwards all calls to it by mirroring its API.
	 */
	struct FExecutionContextProxy
	{
		explicit FExecutionContextProxy(const FExecutionContext& InExecutionContext)
			: ExecutionContext(InExecutionContext)
		{
		}

		// We safely coerce to our wrapped execution context to allow identical usage
		operator const FExecutionContext& () const
		{
			return ExecutionContext;
		}

		// Returns whether or not this execution context is bound to a graph instance
		bool IsBound() const
		{
			return ExecutionContext.IsBound();
		}

		// Returns whether or not this execution context is bound to the specified graph instance
		bool IsBoundTo(const FAnimNextGraphInstancePtr& InGraphInstance) const
		{
			return ExecutionContext.IsBoundTo(InGraphInstance);
		}

		// Returns whether or not this execution context is bound to the specified graph instance
		bool IsBoundTo(const FAnimNextGraphInstance& InGraphInstance) const
		{
			return ExecutionContext.IsBoundTo(InGraphInstance);
		}

		// Queries a node for a trait that implements the specified interface.
		// If no such trait exists, nullptr is returned.
		template<class TraitInterface>
		bool GetInterface(const FWeakTraitPtr& TraitPtr, TTraitBinding<TraitInterface>& InterfaceBinding) const
		{
			return ExecutionContext.GetInterface<TraitInterface>(TraitPtr, InterfaceBinding);
		}

		// Queries a node for a trait that implements the specified interface.
		// If no such trait exists, nullptr is returned.
		template<class TraitInterface>
		bool GetInterface(const FTraitBinding& Binding, TTraitBinding<TraitInterface>& InterfaceBinding) const
		{
			return ExecutionContext.GetInterface<TraitInterface>(Binding, InterfaceBinding);
		}

		// Queries a node for a trait lower on the stack that implements the specified interface.
		// If no such trait exists, nullptr is returned.
		template<class TraitInterface>
		bool GetInterfaceSuper(const FWeakTraitPtr& TraitPtr, TTraitBinding<TraitInterface>& SuperBinding) const
		{
			return ExecutionContext.GetInterfaceSuper<TraitInterface>(TraitPtr, SuperBinding);
		}

		// Queries a node for a trait lower on the stack that implements the specified interface.
		// If no such trait exists, nullptr is returned.
		template<class TraitInterface>
		bool GetInterfaceSuper(const FTraitBinding& Binding, TTraitBinding<TraitInterface>& SuperBinding) const
		{
			return ExecutionContext.GetInterfaceSuper<TraitInterface>(Binding, SuperBinding);
		}

		// Allocates a new node instance from a trait handle
		// If the desired trait lives in the current parent, a weak handle to it will be returned
		FTraitPtr AllocateNodeInstance(const FTraitBinding& ParentBinding, FAnimNextTraitHandle ChildTraitHandle) const
		{
			return ExecutionContext.AllocateNodeInstance(ParentBinding, ChildTraitHandle);
		}

		// Allocates a new node instance from a trait handle
		// If the desired trait lives in the current parent, a weak handle to it will be returned
		FTraitPtr AllocateNodeInstance(const FWeakTraitPtr& ParentBinding, FAnimNextTraitHandle ChildTraitHandle) const
		{
			return ExecutionContext.AllocateNodeInstance(ParentBinding, ChildTraitHandle);
		}

		// Decrements the reference count of the provided node pointer and releases it if
		// there are no more references remaining, reseting the pointer in the process
		void ReleaseNodeInstance(FTraitPtr& NodePtr) const
		{
			return ExecutionContext.ReleaseNodeInstance(NodePtr);
		}

		// Takes a snapshot of all latent properties on the provided node sub-stack (all traits on the sub-stack of the provided one)
		// Properties can be marked as always updating or as supporting freezing (e.g. when a branch of the graph blends out)
		// A freezable property does not update when a snapshot is taken of a frozen node
		void SnapshotLatentProperties(const FWeakTraitPtr& TraitPtr, bool bIsFrozen) const
		{
			ExecutionContext.SnapshotLatentProperties(TraitPtr, bIsFrozen);
		}

		// Returns a typed graph instance component, creating it lazily the first time it is queried
		template<class ComponentType>
		ComponentType& GetComponent() const
		{
			return ExecutionContext.GetComponent<ComponentType>();
		}

		// Returns a typed graph instance component pointer if found or nullptr otherwise
		template<class ComponentType>
		ComponentType* TryGetComponent() const
		{
			return ExecutionContext.TryGetComponent<ComponentType>();
		}

		// Returns const iterators to the graph instance component container
		GraphInstanceComponentMapType::TConstIterator GetComponentIterator() const
		{
			return ExecutionContext.GetComponentIterator();
		}

		// Returns the bound graph instance
		FAnimNextGraphInstance& GetGraphInstance() const
		{
			return ExecutionContext.GetGraphInstance();
		}

	private:
		// The execution context that we wrap
		const FExecutionContext& ExecutionContext;
	};
}
