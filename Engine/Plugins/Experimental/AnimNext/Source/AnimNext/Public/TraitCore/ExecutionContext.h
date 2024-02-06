// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TraitCore/TraitBinding.h"
#include "TraitCore/TraitHandle.h"
#include "TraitCore/TraitPtr.h"
#include "TraitCore/TraitInterfaceUID.h"
#include "TraitCore/LatentPropertyHandle.h"
#include "TraitCore/NodeHandle.h"
#include "Graph/AnimNextGraphInstancePtr.h"

struct FAnimNextGraphInstance;

namespace UE::AnimNext
{
	struct FNodeDescription;
	struct FNodeInstance;
	struct FNodeTemplateRegistry;
	struct FNodeTemplate;
	struct FTrait;
	struct FTraitRegistry;
	struct FTraitTemplate;

	/**
	 * Execution Context
	 * 
	 * The execution context aims to centralize the trait query API.
	 * It is meant to be bound to a graph instance and re-used by the nodes/traits within.
	 */
	struct ANIMNEXT_API FExecutionContext
	{
		// Creates an unbound execution context
		FExecutionContext();

		// Creates an execution context and binds it to the specified graph instance
		explicit FExecutionContext(FAnimNextGraphInstancePtr& InGraphInstance);

		// Creates an execution context and binds it to the specified graph instance
		explicit FExecutionContext(FAnimNextGraphInstance& InGraphInstance);

		// Binds the execution context to the specified graph instance if it differs from the currently bound instance
		void BindTo(FAnimNextGraphInstancePtr& InGraphInstance);

		// Binds the execution context to the specified graph instance if it differs from the currently bound instance
		void BindTo(FAnimNextGraphInstance& InGraphInstance);

		// Binds the execution context to the graph instance that owns the specified trait if it differs from the currently bound instance
		void BindTo(const FWeakTraitPtr& TraitPtr);

		// Returns whether or not this execution context is bound to a graph instance
		bool IsBound() const;

		// Returns whether or not this execution context is bound to the specified graph instance
		bool IsBoundTo(const FAnimNextGraphInstancePtr& InGraphInstance) const;

		// Returns whether or not this execution context is bound to the specified graph instance
		bool IsBoundTo(const FAnimNextGraphInstance& InGraphInstance) const;

		// Queries a node for a trait that implements the specified interface.
		// If no such trait exists, nullptr is returned.
		template<class TraitInterface>
		bool GetInterface(const FWeakTraitPtr& TraitPtr, TTraitBinding<TraitInterface>& InterfaceBinding) const;

		// Queries a node for a trait that implements the specified interface.
		// If no such trait exists, nullptr is returned.
		template<class TraitInterface>
		bool GetInterface(const FTraitBinding& Binding, TTraitBinding<TraitInterface>& InterfaceBinding) const;

		// Queries a node for a trait lower on the stack that implements the specified interface.
		// If no such trait exists, nullptr is returned.
		template<class TraitInterface>
		bool GetInterfaceSuper(const FWeakTraitPtr& TraitPtr, TTraitBinding<TraitInterface>& SuperBinding) const;

		// Queries a node for a trait lower on the stack that implements the specified interface.
		// If no such trait exists, nullptr is returned.
		template<class TraitInterface>
		bool GetInterfaceSuper(const FTraitBinding& Binding, TTraitBinding<TraitInterface>& SuperBinding) const;

		// Allocates a new node instance from a trait handle
		// If the desired trait lives in the current parent, a weak handle to it will be returned
		FTraitPtr AllocateNodeInstance(const FTraitBinding& ParentBinding, FAnimNextTraitHandle ChildTraitHandle) const;

		// Allocates a new node instance from a trait handle
		// If the desired trait lives in the current parent, a weak handle to it will be returned
		FTraitPtr AllocateNodeInstance(const FWeakTraitPtr& ParentBinding, FAnimNextTraitHandle ChildTraitHandle) const;

		// Decrements the reference count of the provided node pointer and releases it if
		// there are no more references remaining, reseting the pointer in the process
		void ReleaseNodeInstance(FTraitPtr& NodePtr) const;

		// Takes a snapshot of all latent properties on the provided node sub-stack (all traits on the sub-stack of the provided one)
		// Properties can be marked as always updating or as supporting freezing (e.g. when a branch of the graph blends out)
		// A freezable property does not update when a snapshot is taken of a frozen node
		void SnapshotLatentProperties(const FWeakTraitPtr& TraitPtr, bool bIsFrozen) const;

		// Returns a typed graph instance component, creating it lazily the first time it is queried
		template<class ComponentType>
		ComponentType& GetComponent() const;

		// Returns a typed graph instance component pointer if found or nullptr otherwise
		template<class ComponentType>
		ComponentType* TryGetComponent() const;

		// Returns const iterators to the graph instance component container
		GraphInstanceComponentMapType::TConstIterator GetComponentIterator() const;

		// Returns the bound graph instance
		FAnimNextGraphInstance& GetGraphInstance() const { return *GraphInstance; }

	private:
		// No copy or move
		FExecutionContext(const FExecutionContext&) = delete;
		FExecutionContext& operator=(const FExecutionContext&) = delete;

		bool GetInterfaceImpl(FTraitInterfaceUID InterfaceUID, const FWeakTraitPtr& TraitPtr, FTraitBinding& InterfaceBinding) const;
		bool GetInterfaceSuperImpl(FTraitInterfaceUID InterfaceUID, const FWeakTraitPtr& TraitPtr, FTraitBinding& SuperBinding) const;
		FGraphInstanceComponent* TryGetComponent(int32 ComponentNameHash, FName ComponentName) const;
		FGraphInstanceComponent& AddComponent(int32 ComponentNameHash, FName ComponentName, TSharedPtr<FGraphInstanceComponent>&& Component) const;

		const FNodeDescription& GetNodeDescription(FNodeHandle NodeHandle) const;
		const FNodeTemplate* GetNodeTemplate(const FNodeDescription& NodeDesc) const;
		const FTrait* GetTrait(const FTraitTemplate& TraitDesc) const;

		// Cached references to the registries we need
		const FNodeTemplateRegistry& NodeTemplateRegistry;
		const FTraitRegistry& TraitRegistry;

		// Cached properties for the currently executing graph
		FAnimNextGraphInstance* GraphInstance = nullptr;
		TArrayView<const uint8> GraphSharedData;
	};

	//////////////////////////////////////////////////////////////////////////
	// Inline implementations

	template<class TraitInterface>
	inline bool FExecutionContext::GetInterface(const FWeakTraitPtr& TraitPtr, TTraitBinding<TraitInterface>& InterfaceBinding) const
	{
		constexpr FTraitInterfaceUID InterfaceUID = TraitInterface::InterfaceUID;
		return GetInterfaceImpl(InterfaceUID, TraitPtr, InterfaceBinding);
	}

	template<class TraitInterface>
	inline bool FExecutionContext::GetInterface(const FTraitBinding& Binding, TTraitBinding<TraitInterface>& InterfaceBinding) const
	{
		return GetInterface<TraitInterface>(Binding.GetTraitPtr(), InterfaceBinding);
	}

	template<class TraitInterface>
	inline bool FExecutionContext::GetInterfaceSuper(const FWeakTraitPtr& TraitPtr, TTraitBinding<TraitInterface>& SuperBinding) const
	{
		constexpr FTraitInterfaceUID InterfaceUID = TraitInterface::InterfaceUID;
		return GetInterfaceSuperImpl(InterfaceUID, TraitPtr, SuperBinding);
	}

	template<class TraitInterface>
	inline bool FExecutionContext::GetInterfaceSuper(const FTraitBinding& Binding, TTraitBinding<TraitInterface>& SuperBinding) const
	{
		return GetInterfaceSuper<TraitInterface>(Binding.GetTraitPtr(), SuperBinding);
	}

	inline FTraitPtr FExecutionContext::AllocateNodeInstance(const FTraitBinding& ParentBinding, FAnimNextTraitHandle ChildTraitHandle) const
	{
		return AllocateNodeInstance(ParentBinding.GetTraitPtr(), ChildTraitHandle);
	}

	template<class ComponentType>
	ComponentType& FExecutionContext::GetComponent() const
	{
		const FName ComponentName = ComponentType::StaticComponentName();
		const int32 ComponentNameHash = GetTypeHash(ComponentName);

		if (FGraphInstanceComponent* Component = TryGetComponent(ComponentNameHash, ComponentName))
		{
			return *static_cast<ComponentType*>(Component);
		}

		return static_cast<ComponentType&>(AddComponent(ComponentNameHash, ComponentName, MakeShared<ComponentType>()));
	}

	template<class ComponentType>
	ComponentType* FExecutionContext::TryGetComponent() const
	{
		const FName ComponentName = ComponentType::StaticComponentName();
		const int32 ComponentNameHash = GetTypeHash(ComponentName);

		return static_cast<ComponentType*>(TryGetComponent(ComponentNameHash, ComponentName));
	}
}
