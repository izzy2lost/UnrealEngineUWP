// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DecoratorBase/DecoratorBinding.h"
#include "DecoratorBase/DecoratorHandle.h"
#include "DecoratorBase/DecoratorPtr.h"
#include "DecoratorBase/DecoratorInterfaceUID.h"
#include "DecoratorBase/LatentPropertyHandle.h"
#include "DecoratorBase/NodeHandle.h"
#include "Graph/AnimNextGraph.h"

namespace UE::AnimNext
{
	struct FNodeDescription;
	struct FNodeInstance;
	struct FNodeTemplateRegistry;
	struct FNodeTemplate;
	struct FDecorator;
	struct FDecoratorRegistry;
	struct FDecoratorTemplate;

	/**
	 * Execution Context
	 * 
	 * The execution context holds internal state during traversals of the animation graph.
	 */
	struct ANIMNEXT_API FExecutionContext
	{
		// Creates an execution context for the specified graph instance
		explicit FExecutionContext(FAnimNextGraphInstance& InGraphInstance);

		// Destroys the execution context
		~FExecutionContext();

		// Queries a node for a decorator that implements the specified interface.
		// If no such decorator exists, nullptr is returned.
		template<class DecoratorInterface>
		bool GetInterface(const FWeakDecoratorPtr& DecoratorPtr, TDecoratorBinding<DecoratorInterface>& InterfaceBinding) const;

		// Queries a node for a decorator that implements the specified interface.
		// If no such decorator exists, nullptr is returned.
		template<class DecoratorInterface>
		bool GetInterface(const FDecoratorBinding& Binding, TDecoratorBinding<DecoratorInterface>& InterfaceBinding) const;

		// Queries a node for a decorator lower on the stack that implements the specified interface.
		// If no such decorator exists, nullptr is returned.
		template<class DecoratorInterface>
		bool GetInterfaceSuper(const FWeakDecoratorPtr& DecoratorPtr, TDecoratorBinding<DecoratorInterface>& SuperBinding) const;

		// Queries a node for a decorator lower on the stack that implements the specified interface.
		// If no such decorator exists, nullptr is returned.
		template<class DecoratorInterface>
		bool GetInterfaceSuper(const FDecoratorBinding& Binding, TDecoratorBinding<DecoratorInterface>& SuperBinding) const;

		// Allocates a new node instance from a decorator handle
		// If the desired decorator lives in the current parent, a weak handle to it will be returned
		FDecoratorPtr AllocateNodeInstance(const FDecoratorBinding& ParentBinding, FAnimNextDecoratorHandle ChildDecoratorHandle) const;

		// Allocates a new node instance from a decorator handle
		// If the desired decorator lives in the current parent, a weak handle to it will be returned
		FDecoratorPtr AllocateNodeInstance(const FWeakDecoratorPtr& ParentBinding, FAnimNextDecoratorHandle ChildDecoratorHandle) const;

		// Releases a node instance that is no longer referenced
		void ReleaseNodeInstance(FNodeInstance* Node) const;

		// Evaluates the latent pin with the specified handle
		template<typename LatentPinType>
		LatentPinType EvaluateLatentPin(FLatentPropertyHandle LatentPropertyHandle) const;

		// Returns a typed graph instance component, creating it lazily the first time it is queried
		template<class ComponentType>
		ComponentType& GetComponent() const;

		// Returns a typed graph instance component pointer if found or nullptr otherwise
		template<class ComponentType>
		ComponentType* TryGetComponent() const;

		// Returns const iterators to the graph instance component container
		GraphInstanceComponentMapType::TConstIterator GetComponentIterator() const { return GraphInstance->GetComponentIterator(); }

		// Returns the bound graph instance
		FAnimNextGraphInstance& GetGraphInstance() const { return *GraphInstance; }

	private:
		// No copy or move
		FExecutionContext(const FExecutionContext&) = delete;
		FExecutionContext& operator=(const FExecutionContext&) = delete;

		bool GetInterfaceImpl(FDecoratorInterfaceUID InterfaceUID, const FWeakDecoratorPtr& DecoratorPtr, FDecoratorBinding& InterfaceBinding) const;
		bool GetInterfaceSuperImpl(FDecoratorInterfaceUID InterfaceUID, const FWeakDecoratorPtr& DecoratorPtr, FDecoratorBinding& SuperBinding) const;
		void EvaluateLatentPinImpl(FLatentPropertyHandle LatentPropertyHandle, void* DestinationPtr) const;

		const FNodeDescription& GetNodeDescription(FNodeHandle NodeHandle) const;
		const FNodeTemplate* GetNodeTemplate(const FNodeDescription& NodeDesc) const;
		const FDecorator* GetDecorator(const FDecoratorTemplate& DecoratorDesc) const;

		// Cached references to the registries we need
		const FNodeTemplateRegistry& NodeTemplateRegistry;
		const FDecoratorRegistry& DecoratorRegistry;

		// Cached properties for the currently executing graph
		const UAnimNextGraph* Graph = nullptr;
		FAnimNextGraphInstance* GraphInstance = nullptr;
		TArrayView<const uint8> GraphSharedData;
	};

	// Returns a pointer to the current execution context if present, nullptr otherwise.
	FExecutionContext* GetThreadExecutionContext();

	//////////////////////////////////////////////////////////////////////////
	// Inline implementations

	template<class DecoratorInterface>
	inline bool FExecutionContext::GetInterface(const FWeakDecoratorPtr& DecoratorPtr, TDecoratorBinding<DecoratorInterface>& InterfaceBinding) const
	{
		constexpr FDecoratorInterfaceUID InterfaceUID = DecoratorInterface::InterfaceUID;
		return GetInterfaceImpl(InterfaceUID, DecoratorPtr, InterfaceBinding);
	}

	template<class DecoratorInterface>
	inline bool FExecutionContext::GetInterface(const FDecoratorBinding& Binding, TDecoratorBinding<DecoratorInterface>& InterfaceBinding) const
	{
		return GetInterface<DecoratorInterface>(Binding.GetDecoratorPtr(), InterfaceBinding);
	}

	template<class DecoratorInterface>
	inline bool FExecutionContext::GetInterfaceSuper(const FWeakDecoratorPtr& DecoratorPtr, TDecoratorBinding<DecoratorInterface>& SuperBinding) const
	{
		constexpr FDecoratorInterfaceUID InterfaceUID = DecoratorInterface::InterfaceUID;
		return GetInterfaceSuperImpl(InterfaceUID, DecoratorPtr, SuperBinding);
	}

	template<class DecoratorInterface>
	inline bool FExecutionContext::GetInterfaceSuper(const FDecoratorBinding& Binding, TDecoratorBinding<DecoratorInterface>& SuperBinding) const
	{
		return GetInterfaceSuper<DecoratorInterface>(Binding.GetDecoratorPtr(), SuperBinding);
	}

	inline FDecoratorPtr FExecutionContext::AllocateNodeInstance(const FDecoratorBinding& ParentBinding, FAnimNextDecoratorHandle ChildDecoratorHandle) const
	{
		return AllocateNodeInstance(ParentBinding.GetDecoratorPtr(), ChildDecoratorHandle);
	}

	template<typename LatentPinType>
	inline LatentPinType FExecutionContext::EvaluateLatentPin(FLatentPropertyHandle LatentPropertyHandle) const
	{
		LatentPinType Result{};

		// Latent pin handle needs to be valid
		if (ensure(LatentPropertyHandle.IsValid()))
		{
			EvaluateLatentPinImpl(LatentPropertyHandle, &Result);
		}

		return Result;
	}

	template<class ComponentType>
	ComponentType& FExecutionContext::GetComponent() const
	{
		return GraphInstance->GetComponent<ComponentType>();
	}

	template<class ComponentType>
	ComponentType* FExecutionContext::TryGetComponent() const
	{
		return GraphInstance->TryGetComponent<ComponentType>();
	}
}
