// Copyright Epic Games, Inc. All Rights Reserved.

#include "DecoratorBase/DecoratorWriter.h"

#if WITH_EDITOR
#include "DecoratorBase/DecoratorRegistry.h"
#include "DecoratorBase/DecoratorTemplate.h"
#include "DecoratorBase/NodeDescription.h"
#include "DecoratorBase/NodeTemplate.h"
#include "DecoratorBase/NodeTemplateRegistry.h"
#include "Serialization/ArchiveUObject.h"

#include "DecoratorBase/DecoratorReader.h"

namespace UE::AnimNext
{
	FDecoratorWriter::FDecoratorWriter()
		: FMemoryWriter(GraphSharedDataArchiveBuffer)
		, NextNodeID(FNodeID::GetFirstID())
		, NumNodesWritten(0)
		, bIsNodeWriting(false)
		, ErrorState(EErrorState::None)
	{
	}

	FNodeHandle FDecoratorWriter::RegisterNode(const FNodeTemplate& NodeTemplate)
	{
		ensure(!bIsNodeWriting);

		if (ErrorState != EErrorState::None)
		{
			// We encountered an error, do nothing
			return FNodeHandle();
		}

		if (NodeTemplate.GetNodeTemplateSize() > FNodeTemplate::MAXIMUM_SIZE)
		{
			// This node template is too large
			ErrorState = EErrorState::NodeTemplateTooLarge;
			return FNodeHandle();
		}

		if (!NextNodeID.IsValid())
		{
			// We have too many nodes in the graph, we need to be able to represent them with 16 bits
			// The node ID must have wrapped around
			ErrorState = EErrorState::TooManyNodes;
			return FNodeHandle();
		}

		const FNodeHandle NodeHandle = FNodeHandle::FromNodeID(NextNodeID);
		check(NodeHandle.IsValid() && NodeHandle.IsNodeID());
		check(NodeMappings.Num() == NodeHandle.GetNodeID().GetNodeIndex());

		NextNodeID = NextNodeID.GetNextID();

		FNodeTemplateRegistry& NodeTemplateRegistry = FNodeTemplateRegistry::Get();
		const FNodeTemplateRegistryHandle NodeTemplateHandle = NodeTemplateRegistry.FindOrAdd(&NodeTemplate);

		NodeMappings.Add({ NodeHandle, NodeTemplateHandle, 0 });

		return NodeHandle;
	}

	void FDecoratorWriter::BeginNodeWriting()
	{
		ensure(!bIsNodeWriting);
		ensure(NumNodesWritten == 0);

		if (ErrorState != EErrorState::None)
		{
			// We encountered an error, do nothing
			return;
		}

		bIsNodeWriting = true;
		GraphReferencedObjects.Reset();
		CurrentLatentPropertyHandle = FLatentPropertyHandle::GetFirstHandle();

		// Serialize the node templates
		TArray<FNodeTemplateRegistryHandle> NodeTemplateHandles;
		NodeTemplateHandles.Reserve(NodeMappings.Num());

		for (FNodeMapping& NodeMapping : NodeMappings)
		{
			NodeMapping.NodeTemplateIndex = NodeTemplateHandles.AddUnique(NodeMapping.NodeTemplateHandle);
		}

		uint32 NumNodeTemplates = NodeTemplateHandles.Num();
		*this << NumNodeTemplates;

		FNodeTemplateRegistry& NodeTemplateRegistry = FNodeTemplateRegistry::Get();
		for (FNodeTemplateRegistryHandle NodeTemplateHandle : NodeTemplateHandles)
		{
			FNodeTemplate* NodeTemplate = NodeTemplateRegistry.FindMutable(NodeTemplateHandle);
			NodeTemplate->Serialize(*this);
		}

		// Begin serializing the graph shared data
		uint32 NumNodes = NodeMappings.Num();
		*this << NumNodes;

		// Serialize the node template indices that we'll use for each node
		for (const FNodeMapping& NodeMapping : NodeMappings)
		{
			uint32 NodeTemplateIndex = NodeMapping.NodeTemplateIndex;
			*this << NodeTemplateIndex;
		}
	}

	void FDecoratorWriter::EndNodeWriting()
	{
		ensure(bIsNodeWriting);
		bIsNodeWriting = false;

		if (ErrorState != EErrorState::None)
		{
			// We encountered an error, do nothing
			return;
		}

		ensure(NumNodesWritten == NodeMappings.Num());
	}

	void FDecoratorWriter::WriteNode(
		const FNodeHandle NodeHandle,
		const TFunction<FString (uint32 DecoratorIndex, const FString& PropertyName)>& GetDecoratorProperty,
		const TFunction<bool(uint32 DecoratorIndex, const FString& PropertyName)>& IsDecoratorPropertyLatent)
	{
		ensure(bIsNodeWriting);

		if (ErrorState != EErrorState::None)
		{
			// We encountered an error, do nothing
			return;
		}

		const FDecoratorRegistry& DecoratorRegistry = FDecoratorRegistry::Get();
		FNodeTemplateRegistry& NodeTemplateRegistry = FNodeTemplateRegistry::Get();

		const FNodeMapping* NodeMapping = NodeMappings.FindByPredicate([NodeHandle](const FNodeMapping& It) { return It.NodeHandle == NodeHandle; });
		if (NodeMapping == nullptr)
		{
			ErrorState = EErrorState::NodeHandleNotFound;
			return;
		}

		const FNodeTemplate* NodeTemplate = NodeTemplateRegistry.Find(NodeMapping->NodeTemplateHandle);
		if (NodeTemplate == nullptr)
		{
			ErrorState = EErrorState::NodeTemplateNotFound;
			return;
		}

		// Populate our node description into a temporary buffer
		alignas(16) uint8 Buffer[64 * 1024];	// Max node size

		FNodeDescription* NodeDesc = new(Buffer) FNodeDescription(NodeMapping->NodeHandle.GetNodeID(), NodeMapping->NodeTemplateHandle);

		// Populate our decorator properties
		const uint32 NumDecorators = NodeTemplate->GetNumDecorators();
		const FDecoratorTemplate* DecoratorTemplates = NodeTemplate->GetDecorators();
		for (uint32 DecoratorIndex = 0; DecoratorIndex < NumDecorators; ++DecoratorIndex)
		{
			const FDecoratorRegistryHandle DecoratorHandle = DecoratorTemplates[DecoratorIndex].GetRegistryHandle();
			const FDecorator* Decorator = DecoratorRegistry.Find(DecoratorHandle);

			FAnimNextDecoratorSharedData* SharedData = DecoratorTemplates[DecoratorIndex].GetDecoratorDescription(*NodeDesc);

			// Curry our lambda with the decorator index
			const auto GetDecoratorPropertyAt = [&GetDecoratorProperty, DecoratorIndex](const FString& PropertyName)
			{
				return GetDecoratorProperty(DecoratorIndex, PropertyName);
			};

			Decorator->SaveDecoratorSharedData(GetDecoratorPropertyAt, *SharedData);
		}

		// Append our node and decorator shared data to our archive
		NodeDesc->Serialize(*this);

		// Append our decorator latent property handles to our archive
		for (uint32 DecoratorIndex = 0; DecoratorIndex < NumDecorators; ++DecoratorIndex)
		{
			const FDecoratorRegistryHandle DecoratorHandle = DecoratorTemplates[DecoratorIndex].GetRegistryHandle();
			const FDecorator* Decorator = DecoratorRegistry.Find(DecoratorHandle);

			// Curry our lambda with the decorator index
			const auto IsDecoratorPropertyLatentAt = [&IsDecoratorPropertyLatent, DecoratorIndex](const FString& PropertyName)
			{
				return IsDecoratorPropertyLatent(DecoratorIndex, PropertyName);
			};

			const TArray<FLatentPropertyHandle> LatentHandles = Decorator->GetLatentPropertyHandles(IsFilterEditorOnly(), IsDecoratorPropertyLatentAt, CurrentLatentPropertyHandle);

			int32 NumLatentHandles = LatentHandles.Num();
			*this << NumLatentHandles;

			for (FLatentPropertyHandle Handle : LatentHandles)
			{
				*this << Handle;
			}
		}

		NumNodesWritten++;
	}

	FDecoratorWriter::EErrorState FDecoratorWriter::GetErrorState() const
	{
		return ErrorState;
	}

	const TArray<uint8>& FDecoratorWriter::GetGraphSharedData() const
	{
		return GraphSharedDataArchiveBuffer;
	}

	const TArray<UObject*>& FDecoratorWriter::GetGraphReferencedObjects() const
	{
		return GraphReferencedObjects;
	}

	FArchive& FDecoratorWriter::operator<<(UObject*& Obj)
	{
		// Add our object for tracking
		int32 ObjectIndex = GraphReferencedObjects.AddUnique(Obj);

		// Save our index, we'll use it to resolve the object on load
		*this << ObjectIndex;

		return *this;
	}

	FArchive& FDecoratorWriter::operator<<(FObjectPtr& Obj)
	{
		return FArchiveUObject::SerializeObjectPtr(*this, Obj);
	}
}
#endif
