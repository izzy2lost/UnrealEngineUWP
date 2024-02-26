// Copyright Epic Games, Inc. All Rights Reserved.

#include "TraitCore/ExecutionContext.h"

#include "TraitCore/Trait.h"
#include "TraitCore/TraitRegistry.h"
#include "TraitCore/TraitTemplate.h"
#include "TraitCore/NodeDescription.h"
#include "TraitCore/NodeInstance.h"
#include "TraitCore/NodeTemplate.h"
#include "TraitCore/NodeTemplateRegistry.h"
#include "Graph/AnimNextGraph.h"
#include "Graph/AnimNextGraphInstance.h"

namespace UE::AnimNext
{
	FExecutionContext::FExecutionContext()
		: MemStack(FMemStack::Get())
		, NodeTemplateRegistry(FNodeTemplateRegistry::Get())
		, TraitRegistry(FTraitRegistry::Get())
	{
	}

	FExecutionContext::FExecutionContext(FAnimNextGraphInstancePtr& InGraphInstance)
		: FExecutionContext()
	{
		BindTo(InGraphInstance);
	}

	FExecutionContext::FExecutionContext(FAnimNextGraphInstance& InGraphInstance)
		: FExecutionContext()
	{
		BindTo(InGraphInstance);
	}

	void FExecutionContext::BindTo(FAnimNextGraphInstancePtr& InGraphInstance)
	{
		if (FAnimNextGraphInstance* InstanceImpl = InGraphInstance.GetImpl())
		{
			BindTo(*InstanceImpl);
		}
	}

	void FExecutionContext::BindTo(FAnimNextGraphInstance& InGraphInstance)
	{
		FAnimNextGraphInstance* InRootGraphInstance = InGraphInstance.GetRootGraphInstance();
		if (RootGraphInstance == InRootGraphInstance)
		{
			return;	// Already bound to this root graph instance, nothing to do
		}

		RootGraphInstance = InRootGraphInstance;
	}

	void FExecutionContext::BindTo(const FWeakTraitPtr& TraitPtr)
	{
		if (const FNodeInstance* NodeInstance = TraitPtr.GetNodeInstance())
		{
			BindTo(NodeInstance->GetOwner());
		}
	}

	bool FExecutionContext::IsBound() const
	{
		return RootGraphInstance != nullptr;
	}

	bool FExecutionContext::IsBoundTo(const FAnimNextGraphInstancePtr& InGraphInstance) const
	{
		if (FAnimNextGraphInstance* GraphInstance = InGraphInstance.GetImpl())
		{
			return RootGraphInstance == GraphInstance->GetRootGraphInstance();
		}

		return false;
	}

	bool FExecutionContext::IsBoundTo(const FAnimNextGraphInstance& InGraphInstance) const
	{
		return RootGraphInstance == InGraphInstance.GetRootGraphInstance();
	}

	FTraitPtr FExecutionContext::AllocateNodeInstance(FAnimNextGraphInstance& GraphInstance, FAnimNextTraitHandle ChildTraitHandle) const
	{
		if (!ChildTraitHandle.IsValid())
		{
			return FTraitPtr();	// Attempting to allocate a node using an invalid trait handle
		}

		if (!ensure(IsBound()))
		{
			return FTraitPtr();	// The execution context must be bound to a valid graph instance
		}

		if (!ensure(GraphInstance.GetGraph() != nullptr))
		{
			return FTraitPtr();	// We need a valid graph instance to allocate into
		}

		const FNodeHandle ChildNodeHandle = ChildTraitHandle.GetNodeHandle();
		const uint32 ChildTraitIndex = ChildTraitHandle.GetTraitIndex();

		const FNodeDescription& NodeDesc = GetNodeDescription(GraphInstance, ChildNodeHandle);
		const FNodeTemplate* NodeTemplate = GetNodeTemplate(NodeDesc);

		if (!ensure(NodeTemplate != nullptr))
		{
			return FTraitPtr();	// Node template wasn't found, node descriptor is perhaps corrupted
		}
		else if (ChildTraitIndex >= NodeTemplate->GetNumTraits())
		{
			return FTraitPtr();	// The requested trait index doesn't exist on that node descriptor
		}

		// We need to allocate a new node instance
		const uint32 InstanceSize = NodeDesc.GetNodeInstanceDataSize();
		uint8* NodeInstanceBuffer = reinterpret_cast<uint8*>(FMemory::Malloc(InstanceSize, 16));
		FNodeInstance* NodeInstance = new(NodeInstanceBuffer) FNodeInstance(GraphInstance, ChildNodeHandle);

		// Manually bind our stack since we have everything we need
		FTraitStackBinding StackBinding;
		StackBinding.Context = this;
		StackBinding.NodeInstance = NodeInstance;
		StackBinding.NodeDescription = &NodeDesc;
		StackBinding.NodeTemplate = NodeTemplate;

		// Start construction with the base trait
		// We construct the whole node which will include one or more sub-stacks (each with their own base trait)
		const FTraitTemplate* TraitDescs = NodeTemplate->GetTraits();
		const FTraitTemplate* StartDesc = TraitDescs;
		const FTraitTemplate* EndDesc = TraitDescs + NodeTemplate->GetNumTraits();

		for (const FTraitTemplate* TraitDesc = StartDesc; TraitDesc != EndDesc; ++TraitDesc)
		{
			const FTrait* Trait = GetTrait(*TraitDesc);
			if (Trait == nullptr)
			{
				continue;	// Trait hasn't been loaded or registered, skip it
			}

			const uint32 TraitIndex = TraitDesc - TraitDescs;

			if (TraitDesc->GetMode() == ETraitMode::Base)
			{
				// A new base trait, update our stack binding
				StackBinding.BaseTraitIndex = TraitIndex;
				StackBinding.TopTraitIndex = TraitDesc->GetNumStackTraits() - 1;
			}

			const FTraitBinding Binding(&StackBinding, Trait, TraitIndex);
			Trait->ConstructTraitInstance(*this, Binding);
		}

		return FTraitPtr(NodeInstance, ChildTraitIndex);
	}

	FTraitPtr FExecutionContext::AllocateNodeInstance(const FWeakTraitPtr& ParentBinding, FAnimNextTraitHandle ChildTraitHandle) const
	{
		if (!ChildTraitHandle.IsValid())
		{
			return FTraitPtr();	// Attempting to allocate a node using an invalid trait handle
		}

		if (!ensure(IsBound()))
		{
			return FTraitPtr();	// The execution context must be bound to a valid graph instance
		}

		if (!ensure(ParentBinding.IsValid()))
		{
			return FTraitPtr();	// We need a parent binding to know which graph instance to allocate into
		}

		FNodeInstance* ParentNodeInstance = ParentBinding.GetNodeInstance();
		FAnimNextGraphInstance& GraphInstance = ParentNodeInstance->GetOwner();

		const FNodeHandle ChildNodeHandle = ChildTraitHandle.GetNodeHandle();
		const uint32 ChildTraitIndex = ChildTraitHandle.GetTraitIndex();

		const FNodeDescription& NodeDesc = GetNodeDescription(GraphInstance, ChildNodeHandle);
		const FNodeTemplate* NodeTemplate = GetNodeTemplate(NodeDesc);

		if (!ensure(NodeTemplate != nullptr))
		{
			return FTraitPtr();	// Node template wasn't found, node descriptor is perhaps corrupted
		}
		else if (ChildTraitIndex >= NodeTemplate->GetNumTraits())
		{
			return FTraitPtr();	// The requested trait index doesn't exist on that node descriptor
		}

		// If the trait we wish to allocate lives in the parent node, return a weak handle to it
		// We use a weak handle to avoid issues when multiple base traits live within the same node
		// When this happens, a trait can end up pointing to another within the same node causing
		// the reference count to never reach zero when all other handles are released
		if (ParentNodeInstance->GetNodeHandle() == ChildNodeHandle)
		{
			return FTraitPtr(ParentNodeInstance, FTraitPtr::IS_WEAK_BIT, ChildTraitIndex);
		}

		// We need to allocate a new node instance
		const uint32 InstanceSize = NodeDesc.GetNodeInstanceDataSize();
		uint8* NodeInstanceBuffer = reinterpret_cast<uint8*>(FMemory::Malloc(InstanceSize, 16));
		FNodeInstance* NodeInstance = new(NodeInstanceBuffer) FNodeInstance(GraphInstance, ChildNodeHandle);

		// Manually bind our stack since we have everything we need
		FTraitStackBinding StackBinding;
		StackBinding.Context = this;
		StackBinding.NodeInstance = NodeInstance;
		StackBinding.NodeDescription = &NodeDesc;
		StackBinding.NodeTemplate = NodeTemplate;

		// Start construction with the base trait
		// We construct the whole node which will include one or more sub-stacks (each with their own base trait)
		const FTraitTemplate* TraitDescs = NodeTemplate->GetTraits();
		const FTraitTemplate* StartDesc = TraitDescs;
		const FTraitTemplate* EndDesc = TraitDescs + NodeTemplate->GetNumTraits();

		for (const FTraitTemplate* TraitDesc = StartDesc; TraitDesc != EndDesc; ++TraitDesc)
		{
			const FTrait* Trait = GetTrait(*TraitDesc);
			if (Trait == nullptr)
			{
				continue;	// Trait hasn't been loaded or registered, skip it
			}

			const uint32 TraitIndex = TraitDesc - TraitDescs;

			if (TraitDesc->GetMode() == ETraitMode::Base)
			{
				// A new base trait, update our stack binding
				StackBinding.BaseTraitIndex = TraitIndex;
				StackBinding.TopTraitIndex = TraitDesc->GetNumStackTraits() - 1;
			}

			const FTraitBinding Binding(&StackBinding, Trait, TraitIndex);
			Trait->ConstructTraitInstance(*this, Binding);
		}

		return FTraitPtr(NodeInstance, ChildTraitIndex);
	}

	void FExecutionContext::ReleaseNodeInstance(FTraitPtr& NodePtr) const
	{
		if (!NodePtr.IsValid())
		{
			return;
		}

		FNodeInstance* NodeInstance = NodePtr.GetNodeInstance();

		if (!ensure(IsBoundTo(NodeInstance->GetOwner())))
		{
			return;	// The execution context isn't bound to the right graph instance
		}

		// Reset the handle here to simplify the multiple return statements below
		NodePtr.PackedPointerAndFlags = 0;
		NodePtr.TraitIndex = 0;

		if (NodeInstance->RemoveReference())
		{
			return;	// Node instance still has references, we can't release it
		}

		const FNodeDescription& NodeDesc = GetNodeDescription(*NodeInstance);
		const FNodeTemplate* NodeTemplate = GetNodeTemplate(NodeDesc);
		if (!ensure(NodeTemplate != nullptr))
		{
			return;	// Node template wasn't found, node descriptor is perhaps corrupted (we'll leak the node memory)
		}

		// Manually bind our stack since we have everything we need
		FTraitStackBinding StackBinding;
		StackBinding.Context = this;
		StackBinding.NodeInstance = NodeInstance;
		StackBinding.NodeDescription = &NodeDesc;
		StackBinding.NodeTemplate = NodeTemplate;

		// Start destruction with the top trait
		// We destruct the whole node which will include one or more sub-stacks (each with their own base trait)
		const FTraitTemplate* TraitDescs = NodeTemplate->GetTraits();
		const FTraitTemplate* StartDesc = TraitDescs + NodeTemplate->GetNumTraits() - 1;
		const FTraitTemplate* EndDesc = TraitDescs - 1;
		for (const FTraitTemplate* TraitDesc = StartDesc; TraitDesc != EndDesc; --TraitDesc)
		{
			const FTrait* Trait = GetTrait(*TraitDesc);
			if (Trait == nullptr)
			{
				continue;	// Trait hasn't been loaded or registered, skip it
			}

			// Always update our stack binding to make sure it points to the right sub-stack
			const FTraitTemplate* BaseTraitDesc = TraitDesc - TraitDesc->GetTraitIndex();
			StackBinding.BaseTraitIndex = BaseTraitDesc - TraitDescs;
			StackBinding.TopTraitIndex = StackBinding.BaseTraitIndex + BaseTraitDesc->GetNumStackTraits() - 1;

			const uint32 TraitIndex = TraitDesc - TraitDescs;
			const FTraitBinding Binding(&StackBinding, Trait, TraitIndex);
			Trait->DestructTraitInstance(*this, Binding);
		}

		NodeInstance->~FNodeInstance();
		FMemory::Free(NodeInstance);
	}

	bool FExecutionContext::GetStack(const FWeakTraitPtr& TraitPtr, FTraitStackBinding& OutStackBinding) const
	{
		if (!TraitPtr.IsValid())
		{
			OutStackBinding.Reset();
			return false;
		}

		if (!ensure(IsBoundTo(TraitPtr.GetNodeInstance()->GetOwner())))
		{
			OutStackBinding.Reset();
			return false;	// The execution context isn't bound to the right graph instance
		}

		OutStackBinding = FTraitStackBinding(*this, TraitPtr);
		return OutStackBinding.IsValid();	// Construction can fail in rare cases, see constructor
	}

	FGraphInstanceComponent* FExecutionContext::TryGetComponent(int32 ComponentNameHash, FName ComponentName) const
	{
		check(IsBound());
		return RootGraphInstance->TryGetComponent(ComponentNameHash, ComponentName);
	}

	FGraphInstanceComponent& FExecutionContext::AddComponent(int32 ComponentNameHash, FName ComponentName, TSharedPtr<FGraphInstanceComponent>&& Component) const
	{
		check(IsBound());
		return RootGraphInstance->AddComponent(ComponentNameHash, ComponentName, MoveTemp(Component));
	}

	GraphInstanceComponentMapType::TConstIterator FExecutionContext::GetComponentIterator() const
	{
		check(IsBound());
		return RootGraphInstance->GetComponentIterator();
	}

	const FNodeDescription& FExecutionContext::GetNodeDescription(const FAnimNextGraphInstance& GraphInstance, FNodeHandle NodeHandle) const
	{
		// Grab the node description from the specified graph
		const UAnimNextGraph* Graph = GraphInstance.GetGraph();
		return *reinterpret_cast<const FNodeDescription*>(&Graph->SharedDataBuffer[NodeHandle.GetSharedOffset()]);
	}

	const FNodeDescription& FExecutionContext::GetNodeDescription(const FNodeInstance& NodeInstance) const
	{
		// Grab the node description from the owning graph
		return GetNodeDescription(NodeInstance.GetOwner(), NodeInstance.GetNodeHandle());
	}

	const FNodeTemplate* FExecutionContext::GetNodeTemplate(const FNodeDescription& NodeDesc) const
	{
		check(NodeDesc.GetTemplateHandle().IsValid());
		return NodeTemplateRegistry.Find(NodeDesc.GetTemplateHandle());
	}

	const FTrait* FExecutionContext::GetTrait(const FTraitTemplate& Template) const
	{
		check(Template.GetRegistryHandle().IsValid());
		return TraitRegistry.Find(Template.GetRegistryHandle());
	}
}
