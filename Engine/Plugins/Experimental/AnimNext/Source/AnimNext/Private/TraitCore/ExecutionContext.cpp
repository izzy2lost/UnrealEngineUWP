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
		: NodeTemplateRegistry(FNodeTemplateRegistry::Get())
		, TraitRegistry(FTraitRegistry::Get())
		, GraphInstance(nullptr)
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
		if (GraphInstance == &InGraphInstance)
		{
			return;	// Already bound to this graph instance, nothing to do
		}

		if (const UAnimNextGraph* Graph = InGraphInstance.GetGraph())
		{
			GraphInstance = &InGraphInstance;
			GraphSharedData = Graph->SharedDataBuffer;
		}
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
		return GraphInstance != nullptr;
	}

	bool FExecutionContext::IsBoundTo(const FAnimNextGraphInstancePtr& InGraphInstance) const
	{
		return GraphInstance == InGraphInstance.GetImpl();
	}

	bool FExecutionContext::IsBoundTo(const FAnimNextGraphInstance& InGraphInstance) const
	{
		return GraphInstance == &InGraphInstance;
	}

	FTraitPtr FExecutionContext::AllocateNodeInstance(const FWeakTraitPtr& ParentBinding, FAnimNextTraitHandle ChildTraitHandle) const
	{
		if (!ensure(ChildTraitHandle.IsValid()))
		{
			return FTraitPtr();	// Attempting to allocate a node using an invalid trait handle
		}

		if (!ensure(IsBound()))
		{
			return FTraitPtr();	// The execution context must be bound to a valid graph instance
		}

		const FNodeHandle ChildNodeHandle = ChildTraitHandle.GetNodeHandle();
		const FNodeDescription& NodeDesc = GetNodeDescription(ChildNodeHandle);

		const FNodeTemplate* NodeTemplate = GetNodeTemplate(NodeDesc);
		if (!ensure(NodeTemplate != nullptr))
		{
			return FTraitPtr();	// Node template wasn't found, node descriptor is perhaps corrupted
		}

		const uint32 ChildTraitIndex = ChildTraitHandle.GetTraitIndex();

		if (ChildTraitIndex >= NodeTemplate->GetNumTraits())
		{
			return FTraitPtr();	// The requested trait index doesn't exist on that node descriptor
		}

		// If the trait we wish to allocate lives in the parent node, return a weak handle to it
		// We use a weak handle to avoid issues when multiple base traits live within the same node
		// When this happens, a trait can end up pointing to another within the same node causing
		// the reference count to never reach zero when all other handles are released
		if (FNodeInstance* ParentNodeInstance = ParentBinding.GetNodeInstance())
		{
			if (ParentNodeInstance->GetNodeHandle() == ChildNodeHandle)
			{
				return FTraitPtr(ParentNodeInstance, FTraitPtr::IS_WEAK_BIT, ChildTraitIndex);
			}
		}

		// We need to allocate a new node instance
		const FTraitTemplate* TraitDescs = NodeTemplate->GetTraits();

		const uint32 InstanceSize = NodeDesc.GetNodeInstanceDataSize();
		uint8* NodeInstanceBuffer = reinterpret_cast<uint8*>(FMemory::Malloc(InstanceSize, 16));
		FNodeInstance* NodeInstance = new(NodeInstanceBuffer) FNodeInstance(*GraphInstance, ChildNodeHandle);

		// Start construction with the bottom trait
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

			FWeakTraitPtr TraitPtr(NodeInstance, TraitIndex);

			const FAnimNextTraitSharedData* SharedData = TraitDesc->GetTraitDescription(NodeDesc);
			FTraitInstanceData* InstanceData = TraitDesc->GetTraitInstance(*NodeInstance);

			const FTraitBinding Binding(nullptr, TraitDesc, &NodeDesc, TraitPtr);
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

		const FNodeDescription& NodeDesc = GetNodeDescription(NodeInstance->GetNodeHandle());

		const FNodeTemplate* NodeTemplate = GetNodeTemplate(NodeDesc);
		if (!ensure(NodeTemplate != nullptr))
		{
			return;	// Node template wasn't found, node descriptor is perhaps corrupted (we'll leak the node memory)
		}

		const FTraitTemplate* TraitDescs = NodeTemplate->GetTraits();

		// Start destruction with the top trait
		const FTraitTemplate* StartDesc = TraitDescs + NodeTemplate->GetNumTraits() - 1;
		const FTraitTemplate* EndDesc = TraitDescs - 1;
		for (const FTraitTemplate* TraitDesc = StartDesc; TraitDesc != EndDesc; --TraitDesc)
		{
			if (const FTrait* Trait = GetTrait(*TraitDesc))
			{
				const uint32 TraitIndex = TraitDesc - TraitDescs;

				FWeakTraitPtr TraitPtr(NodeInstance, TraitIndex);

				const FTraitBinding Binding(nullptr, TraitDesc, &NodeDesc, TraitPtr);
				Trait->DestructTraitInstance(*this, Binding);
			}
		}

		NodeInstance->~FNodeInstance();
		FMemory::Free(NodeInstance);
	}

	bool FExecutionContext::GetInterfaceImpl(FTraitInterfaceUID InterfaceUID, const FWeakTraitPtr& TraitPtr, FTraitBinding& InterfaceBinding) const
	{
		if (!TraitPtr.IsValid())
		{
			return false;
		}

		// TODO: Revisit this to avoid querying each trait over and over
		//
		// If we have D traits on a node and I unique interfaces per trait then finding an interface
		// has O(D*I) complexity: for each trait we have to test every interface
		// However, if the I interfaces are not unique per trait and instead are unique for the system,
		// the below code will still have the same complexity. We'll end up testing the same interface multiple
		// times, once for each trait. Because we will have a small set of interfaces per node that
		// multiple traits implement (e.g IUpdate, IEvaluate), we can do better.
		// 
		// It would be much cheaper if instead we could test if we implement the interface first and then
		// look for a list of traits (in stack order) that implement it. To do this, when we build the node
		// template, we could aggregate all interfaces that the traits implement (sorted by their interface id
		// for determinism or perhaps some other criteria to put popular/hot interfaces first). Then, in the node
		// template we store a mapping of InterfaceUID to InterfaceIndex (local to that node template). The interface
		// index can then be used to index into a list of trait indices that implement it.
		// Code would look like:
		//    * uint32 InterfaceIndex = NodeTemplate.GetInterfaceUIDs().FindIndex(InterfaceUID);	// INDEX_NONE means that none of the traits implement the interface
		//    * uint16 TraitIndicesStartOffset = NodeTemplate.InterfaceIndexToTraitIndices(InterfaceIndex);		// An offset relative to the NodeTemplate*
		//    * const uint8* TraitIndicesForInterface = ((const uint8*NodeTemplate) + TraitIndicesStartOffset;
		//    * uint8 NumTraitsWithInterface = TraitIndicesForInterface[0];		// Reserve first index for the trait count or if we use a guard value at the end of the array we have to reserve a value
		//    * uint8 TopTraitIndex = TraitIndicesForInterface[1];				// First index is top of stack
		// Super trait lookup would be similar:
		//    * uint8 SuperTraitIndex = TraitIndicesForInterface[TraitIndicesForInterface.FindIndex(CurrentTraitIndex) + 1];	// Need to check if we exceed the max count (bottom of stack)
		// Once we have the trait index, we use the trait template as we do now
		//
		// The above would have a number of advantages:
		//    * Querying for an interface that the node doesn't implement would be much cheaper (we test each interface once)
		//    * InterfaceIndex query can easily be implemented in SIMD, we look for a uint32 in a list sequentially
		//    * Bulk/interleaved querying of interfaces would be much easier (e.g. query for IEvaluate for 4 nodes)
		//    * A lot less branching is involved, improving throughput
		//    * We can handle multiple base traits by checking the trait index found which has comparable cost
		//    * We could store the interface offset from the trait ptr base to avoid the call to GetInterface()
		//      Instead of testing to find our interface again to return a custom static_cast, we'd store the interface/trait offset
		//      in the node template alongside the trait index. We can then query for the FTrait* and add the offset
		//      improving bulk query feasibility.
		//
		// However, querying for a Super interface might be slower since we have to start the search from the start.
		// In contrast, we now start searching at the current trait. In order to keep that cost down and amortize
		// the search, we would have to cache the InterfaceIndex and the current index in the trait list.
		// We could use 8-bits for each entry allowing for a max of 256 interfaces per node and 256 traits per node (existing limitation).
		//
		// Adding new data to the trait binding isn't ideal. It's current size on 64-bit systems is:
		//    * 8 bytes for interface pointer (to forward function calls to)
		//    * 8 bytes for trait template (to get the offsets for shared/instance/latent data)
		//    * 8 bytes for node description pointer (base of shared data)
		//    * 8 bytes for node instance pointer (base of instance data, in weak trait ptr)
		//    * 4 bytes for trait index (in weak trait ptr)
		//    * 4 bytes of padding (in weak trait ptr)
		// We current use 4 bytes for the trait index but in practice we only need 1 byte. We use 4 bytes because we pay
		// for padding anyway. We could use 2 extra bytes for our mapping and still have 5 bytes to space in padding (due to alignment).
		// In practice, trait bindings always live on the stack and they shouldn't be getting copied/moved around.
		// We populate them in place when we query, then we use it from its storage on the stack that we first populated.
		//
		// To be able to support all of this, instead of having FTrait::GetInterface() implemented by derived types
		// we would need instead to implement FTrait::GetInterfaceUIDs(). Then on load, when we build a node template and
		// finalize it, we can build the list of unique interface UIDs and sort it and we can build the mapping structures.
		// On load, as long as we fit within 64KB we are fine. We have to update FNodeTemplate::Finalize and FNodeTemplateBuilder::BuildNodeTemplate.
		// Perhaps the builder should use the same code and build in a buffer on the stack and copy the final size out.
		// We must build the mapping on load because the interfaces we implement are only known at runtime (e.g. can be changed through defines/configuration).
		// We can still implement GetInterface() in terms of GetInterfaceUIDs if it returns the interface offsets as well.

		FNodeInstance* NodeInstance = TraitPtr.GetNodeInstance();

		if (!ensure(IsBoundTo(NodeInstance->GetOwner())))
		{
			return false;	// The execution context isn't bound to the right graph instance
		}

		const FNodeDescription& NodeDesc = GetNodeDescription(NodeInstance->GetNodeHandle());

		const FNodeTemplate* NodeTemplate = GetNodeTemplate(NodeDesc);
		if (!ensure(NodeTemplate != nullptr))
		{
			return false;	// Node template wasn't found, node descriptor is perhaps corrupted
		}

		const FTraitTemplate* TraitDescs = NodeTemplate->GetTraits();

		// We only search within the partial stack of the provided trait
		const FTraitTemplate* CurrentTraitDesc = TraitDescs + TraitPtr.GetTraitIndex();
		const FTraitTemplate* BaseTraitDesc = CurrentTraitDesc->GetMode() == ETraitMode::Base ? CurrentTraitDesc : (CurrentTraitDesc - CurrentTraitDesc->GetAdditiveTraitIndex());

		// Start searching with the top additive trait towards our base trait
		const FTraitTemplate* StartDesc = BaseTraitDesc + BaseTraitDesc->GetNumAdditiveTraits();
		const FTraitTemplate* EndDesc = BaseTraitDesc - 1;
		for (const FTraitTemplate* TraitDesc = StartDesc; TraitDesc != EndDesc; --TraitDesc)
		{
			const FTrait* Trait = GetTrait(*TraitDesc);
			if (Trait == nullptr)
			{
				continue;	// Trait hasn't been loaded or registered, skip it
			}

			if (const ITraitInterface* Interface = Trait->GetTraitInterface(InterfaceUID))
			{
				const FAnimNextTraitSharedData* SharedData = TraitDesc->GetTraitDescription(NodeDesc);
				FTraitInstanceData* InstanceData = TraitDesc->GetTraitInstance(*NodeInstance);

				const uint32 TraitIndex = TraitDesc - TraitDescs;
				FWeakTraitPtr InterfaceTraitPtr(NodeInstance, TraitIndex);

				InterfaceBinding = FTraitBinding(Interface, TraitDesc, &NodeDesc, InterfaceTraitPtr);
				return true;
			}
		}

		// We failed to find a trait that handles this interface
		return false;
	}

	bool FExecutionContext::GetInterfaceSuperImpl(FTraitInterfaceUID InterfaceUID, const FWeakTraitPtr& TraitPtr, FTraitBinding& SuperBinding) const
	{
		if (!TraitPtr.IsValid())
		{
			return false;	// Trait pointer isn't valid, can't find a 'super'
		}

		FNodeInstance* NodeInstance = TraitPtr.GetNodeInstance();

		if (!ensure(IsBoundTo(NodeInstance->GetOwner())))
		{
			return false;	// The execution context isn't bound to the right graph instance
		}

		const FNodeDescription& NodeDesc = GetNodeDescription(NodeInstance->GetNodeHandle());

		const FNodeTemplate* NodeTemplate = GetNodeTemplate(NodeDesc);
		if (!ensure(NodeTemplate != nullptr))
		{
			return false;	// Node template wasn't found, node descriptor is perhaps corrupted
		}

		const FTraitTemplate* TraitDescs = NodeTemplate->GetTraits();

		// We only search within the partial stack of the provided trait
		const FTraitTemplate* CurrentTraitDesc = TraitDescs + TraitPtr.GetTraitIndex();
		if (CurrentTraitDesc->GetMode() == ETraitMode::Base)
		{
			return false;	// We've reached a base trait, we don't forward to any parent trait we might have
		}

		const FTraitTemplate* BaseTraitDesc = CurrentTraitDesc - CurrentTraitDesc->GetAdditiveTraitIndex();

		// Start searching with the next additive trait towards our base trait
		const FTraitTemplate* StartDesc = CurrentTraitDesc - 1;
		const FTraitTemplate* EndDesc = BaseTraitDesc - 1;
		for (const FTraitTemplate* TraitDesc = StartDesc; TraitDesc != EndDesc; --TraitDesc)
		{
			const FTrait* Trait = GetTrait(*TraitDesc);
			if (Trait == nullptr)
			{
				continue;	// Trait hasn't been loaded or registered, skip it
			}

			if (const ITraitInterface* Interface = Trait->GetTraitInterface(InterfaceUID))
			{
				const FAnimNextTraitSharedData* SharedData = TraitDesc->GetTraitDescription(NodeDesc);
				FTraitInstanceData* InstanceData = TraitDesc->GetTraitInstance(*NodeInstance);

				const uint32 TraitIndex = TraitDesc - TraitDescs;
				FWeakTraitPtr SuperPtr(NodeInstance, TraitIndex);

				SuperBinding = FTraitBinding(Interface, TraitDesc, &NodeDesc, SuperPtr);
				return true;
			}
		}

		// We failed to find a trait that handles this interface
		return false;
	}

	void FExecutionContext::SnapshotLatentProperties(const FWeakTraitPtr& TraitPtr, bool bIsFrozen) const
	{
		if (!TraitPtr.IsValid())
		{
			return;	// Nothing to do
		}

		FNodeInstance* NodeInstance = TraitPtr.GetNodeInstance();

		if (!ensure(IsBoundTo(NodeInstance->GetOwner())))
		{
			return;	// The execution context isn't bound to the right graph instance
		}

		const FNodeDescription& NodeDesc = GetNodeDescription(NodeInstance->GetNodeHandle());

		const FNodeTemplate* NodeTemplate = GetNodeTemplate(NodeDesc);
		if (!ensure(NodeTemplate != nullptr))
		{
			return;	// Node template wasn't found, node descriptor is perhaps corrupted
		}

		const FTraitTemplate* TraitDescs = NodeTemplate->GetTraits();

		// We only snapshot the partial stack the specified trait lives in
		const FTraitTemplate* CurrentTraitDesc = TraitDescs + TraitPtr.GetTraitIndex();
		const FTraitTemplate* BaseTraitDesc = CurrentTraitDesc->GetMode() == ETraitMode::Base ? CurrentTraitDesc : (CurrentTraitDesc - CurrentTraitDesc->GetAdditiveTraitIndex());

		const FLatentPropertiesHeader& LatentHeader = BaseTraitDesc->GetTraitLatentPropertiesHeader(NodeDesc);
		if (!LatentHeader.bHasValidLatentProperties)
		{
			return;	// All latent properties are inline, nothing to snapshot
		}
		else if (bIsFrozen && LatentHeader.bCanAllPropertiesFreeze)
		{
			return;	// We are frozen and all latent properties support freezing, nothing to snapshot
		}

		const FLatentPropertyHandle* LatentHandles = BaseTraitDesc->GetTraitLatentPropertyHandles(NodeDesc);
		const uint32 NumLatentHandles = BaseTraitDesc->GetNumSubStackLatentPropreties();

		GraphInstance->ExecuteLatentPins(TConstArrayView<FLatentPropertyHandle>(LatentHandles, NumLatentHandles), NodeInstance, bIsFrozen);
	}

	FGraphInstanceComponent* FExecutionContext::TryGetComponent(int32 ComponentNameHash, FName ComponentName) const
	{
		return GraphInstance->TryGetComponent(ComponentNameHash, ComponentName);
	}

	FGraphInstanceComponent& FExecutionContext::AddComponent(int32 ComponentNameHash, FName ComponentName, TSharedPtr<FGraphInstanceComponent>&& Component) const
	{
		return GraphInstance->AddComponent(ComponentNameHash, ComponentName, MoveTemp(Component));
	}

	GraphInstanceComponentMapType::TConstIterator FExecutionContext::GetComponentIterator() const
	{
		return GraphInstance->GetComponentIterator();
	}

	const FNodeDescription& FExecutionContext::GetNodeDescription(FNodeHandle NodeHandle) const
	{
		check(NodeHandle.IsValid());
		return *reinterpret_cast<const FNodeDescription*>(&GraphSharedData[NodeHandle.GetSharedOffset()]);
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
