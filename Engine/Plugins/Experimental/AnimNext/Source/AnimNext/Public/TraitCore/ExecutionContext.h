// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TraitCore/TraitBinding.h"
#include "TraitCore/TraitHandle.h"
#include "TraitCore/TraitPtr.h"
#include "TraitCore/TraitStackBinding.h"
#include "TraitCore/ITraitInterface.h"
#include "TraitCore/TraitInterfaceUID.h"
#include "TraitCore/LatentPropertyHandle.h"
#include "TraitCore/NodeHandle.h"
#include "Graph/AnimNextGraphInstancePtr.h"
#include "Graph/GraphInstanceComponent.h"

#include <type_traits>

struct FAnimNextGraphInstance;
class FMemStack;

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



		//////////////////////////////////////////////////////////////////////////
		// The following functions handle the context binding
		// In order to be used, the execution context must be bound to a valid root graph instance

		// Binds the execution context to the specified graph instance if it differs from the currently bound instance
		void BindTo(FAnimNextGraphInstancePtr& InGraphInstance);

		// Binds the execution context to the specified graph instance if it differs from the currently bound instance
		void BindTo(FAnimNextGraphInstance& InGraphInstance);

		// Binds the execution context to the graph instance that owns the specified trait if it differs from the currently bound instance
		void BindTo(const FWeakTraitPtr& TraitPtr);

		// Returns whether or not this execution context is bound to a graph instance
		bool IsBound() const;

		// Returns whether or not this execution context is bound to the specified graph instance
		// Returns true if the root graphs match
		bool IsBoundTo(const FAnimNextGraphInstancePtr& InGraphInstance) const;

		// Returns whether or not this execution context is bound to the specified graph instance
		// Returns true if the root graphs match
		bool IsBoundTo(const FAnimNextGraphInstance& InGraphInstance) const;

		//////////////////////////////////////////////////////////////////////////
		// The following functions allow creation of trait stack bindings

		// Returns a trait stack binding to the stack that contains the specified trait pointer.
		// Returns false if we failed to do so.
		bool GetStack(const FWeakTraitPtr& TraitPtr, FTraitStackBinding& OutStackBinding) const;



		//////////////////////////////////////////////////////////////////////////
		// The following functions handle node lifetime management

		// Allocates a new node instance from a trait handle using the specified graph instance
		// If the desired trait lives in the current parent, a weak handle to it will be returned
		FTraitPtr AllocateNodeInstance(FAnimNextGraphInstance& GraphInstance, FAnimNextTraitHandle ChildTraitHandle) const;

		// Allocates a new node instance from a trait handle
		// If the desired trait lives in the current parent, a weak handle to it will be returned
		FTraitPtr AllocateNodeInstance(const FTraitBinding& ParentBinding, FAnimNextTraitHandle ChildTraitHandle) const;

		// Allocates a new node instance from a trait handle
		// If the desired trait lives in the current parent, a weak handle to it will be returned
		FTraitPtr AllocateNodeInstance(const FWeakTraitPtr& ParentBinding, FAnimNextTraitHandle ChildTraitHandle) const;

		// Decrements the reference count of the provided node pointer and releases it if
		// there are no more references remaining, reseting the pointer in the process
		void ReleaseNodeInstance(FTraitPtr& NodePtr) const;



		//////////////////////////////////////////////////////////////////////////
		// The following functions handle graph instance components
		// Graph instance components live on the root graph instance and persist from
		// frame to frame

		// Returns a typed graph instance component, creating it lazily the first time it is queried
		template<class ComponentType>
		ComponentType& GetComponent() const;

		// Returns a typed graph instance component pointer if found or nullptr otherwise
		template<class ComponentType>
		ComponentType* TryGetComponent() const;

		// Returns const iterators to the graph instance component container
		GraphInstanceComponentMapType::TConstIterator GetComponentIterator() const;



		//////////////////////////////////////////////////////////////////////////
		// Misc functions

		// Returns the bound root graph instance
		FAnimNextGraphInstance& GetRootGraphInstance() const { return *RootGraphInstance; }

		// Returns the local thread memstack
		FMemStack& GetMemStack() const { return MemStack; }

	private:
		// No copy or move
		FExecutionContext(const FExecutionContext&) = delete;
		FExecutionContext& operator=(const FExecutionContext&) = delete;

		FGraphInstanceComponent* TryGetComponent(int32 ComponentNameHash, FName ComponentName) const;
		FGraphInstanceComponent& AddComponent(int32 ComponentNameHash, FName ComponentName, TSharedPtr<FGraphInstanceComponent>&& Component) const;

		const FNodeDescription& GetNodeDescription(const FAnimNextGraphInstance& GraphInstance, FNodeHandle NodeHandle) const;
		const FNodeDescription& GetNodeDescription(const FNodeInstance& NodeInstance) const;
		const FNodeTemplate* GetNodeTemplate(const FNodeDescription& NodeDesc) const;
		const FTrait* GetTrait(const FTraitTemplate& TraitDesc) const;

	protected:
		// The memstack of the local thread
		FMemStack& MemStack;

	private:
		// Cached references to the registries we need
		const FNodeTemplateRegistry& NodeTemplateRegistry;
		const FTraitRegistry& TraitRegistry;

		// Root graph instance we are bound to
		FAnimNextGraphInstance* RootGraphInstance = nullptr;

		friend struct FTraitStackBinding;
	};

	//////////////////////////////////////////////////////////////////////////
	// Inline implementations

	inline FTraitPtr FExecutionContext::AllocateNodeInstance(const FTraitBinding& ParentBinding, FAnimNextTraitHandle ChildTraitHandle) const
	{
		return AllocateNodeInstance(ParentBinding.GetTraitPtr(), ChildTraitHandle);
	}

	template<class ComponentType>
	ComponentType& FExecutionContext::GetComponent() const
	{
		static_assert(std::is_base_of<FGraphInstanceComponent, ComponentType>::value, "ComponentType type must derive from FGraphInstanceComponent");

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
		static_assert(std::is_base_of<FGraphInstanceComponent, ComponentType>::value, "ComponentType type must derive from FGraphInstanceComponent");

		const FName ComponentName = ComponentType::StaticComponentName();
		const int32 ComponentNameHash = GetTypeHash(ComponentName);

		return static_cast<ComponentType*>(TryGetComponent(ComponentNameHash, ComponentName));
	}
}
