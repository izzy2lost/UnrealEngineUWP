// Copyright Epic Games, Inc. All Rights Reserved.

#include "DecoratorBase/DecoratorReader.h"

#include "DecoratorBase/NodeDescription.h"
#include "DecoratorBase/NodeInstance.h"
#include "DecoratorBase/NodeTemplate.h"
#include "DecoratorBase/NodeTemplateRegistry.h"
#include "Serialization/ArchiveUObject.h"
#include "UObject/UObjectGlobals.h"

namespace UE::AnimNext
{
	FDecoratorReader::FDecoratorReader(FArchive& Ar)
		: FArchiveProxy(Ar)
	{
	}

	FDecoratorReader::EErrorState FDecoratorReader::ReadGraph(TArray<uint8>& GraphSharedData, TArray<TObjectPtr<UObject>>& TrackedObjectsForGC)
	{
		GraphSharedData.Empty(0);
		TrackedObjectsForGC.Empty(0);

		EErrorState ErrorState = ReadGraphSharedData(GraphSharedData);
		if (ErrorState != EErrorState::None)
		{
			return ErrorState;
		}

		ErrorState = ReadTrackedObjectsForGC(TrackedObjectsForGC);

		return ErrorState;
	}

	FDecoratorReader::EErrorState FDecoratorReader::ReadGraphSharedData(TArray<uint8>& GraphSharedData)
	{
		// Read the node templates and register them as needed
		uint32 NumNodeTemplates = 0;
		*this << NumNodeTemplates;

		TArray<FNodeTemplateRegistryHandle> NodeTemplateHandles;
		NodeTemplateHandles.Reserve(NumNodeTemplates);

		FNodeTemplateRegistry& NodeTemplateRegistry = FNodeTemplateRegistry::Get();

		{
			// We serialize each node template in the same buffer, they get copied into the registry and we don't need to retain them
			alignas(16) uint8 NodeTemplateBuffer[64 * 1024];
			FNodeTemplate* NodeTemplate = reinterpret_cast<FNodeTemplate*>(&NodeTemplateBuffer[0]);

			for (uint32 NodeTemplateIndex = 0; NodeTemplateIndex < NumNodeTemplates; ++NodeTemplateIndex)
			{
				NodeTemplate->Serialize(*this);

				if (!NodeTemplate->IsValid())
				{
					if (NodeTemplate->GetNodeSharedDataSize() == 0)
					{
						// This node shared data is too large
						return EErrorState::NodeSharedDataTooLarge;
					}

					if (NodeTemplate->GetNodeInstanceDataSize() == 0)
					{
						// This node instance data is too large
						return EErrorState::NodeInstanceDataTooLarge;
					}
				}

				// Register our node template
				NodeTemplateHandles.Add(NodeTemplateRegistry.FindOrAdd(NodeTemplate));
			}
		}

		// Read our graph shared data
		uint32 NumNodes = 0;
		*this << NumNodes;

		NodeHandles.Empty(0);
		NodeHandles.Reserve(NumNodes);

		// Calculate our shared data size and all node offsets
		uint32 SharedDataSize = 0;
		for (uint32 NodeIndex = 0; NodeIndex < NumNodes; ++NodeIndex)
		{
			SharedDataSize = Align(SharedDataSize, alignof(FNodeDescription));	// Make sure we respect our alignment constraints

			if (SharedDataSize > MAXIMUM_GRAPH_SHARED_DATA_SIZE)
			{
				// This graph shared data is too large, we won't be able to create handles to this node
				return EErrorState::GraphTooLarge;
			}

			NodeHandles.Add(FNodeHandle::FromSharedOffset(SharedDataSize));	// This node starts here
			check(NodeHandles.Last().IsSharedOffset());

			uint32 NodeTemplateIndex = 0;
			*this << NodeTemplateIndex;

			const FNodeTemplate* NodeTemplate = NodeTemplateRegistry.Find(NodeTemplateHandles[NodeTemplateIndex]);
			check(NodeTemplate != nullptr);

			SharedDataSize += NodeTemplate->GetNodeSharedDataSize();
		}

		// The shared data size might exceed MAXIMUM_GRAPH_SHARED_DATA_SIZE a little bit
		// The only requirement is that the node begins before that threshold so that we can
		// create handles to it

		GraphSharedData.Empty(0);
		GraphSharedData.AddZeroed(SharedDataSize);

		// Serialize our graph shared data
		for (uint32 NodeIndex = 0; NodeIndex < NumNodes; ++NodeIndex)
		{
			// Serialize our node shared data
			const uint32 SharedDataOffset = NodeHandles[NodeIndex].GetSharedOffset();
			FNodeDescription* NodeDesc = reinterpret_cast<FNodeDescription*>(&GraphSharedData[SharedDataOffset]);
			NodeDesc->Serialize(*this);

			// Serialize our latent property handles
			const FNodeTemplate* NodeTemplate = NodeTemplateRegistry.Find(NodeDesc->GetTemplateHandle());

			const uint32 NumDecorators = NodeTemplate->GetNumDecorators();
			const FDecoratorTemplate* DecoratorTemplates = NodeTemplate->GetDecorators();

			for (uint32 DecoratorIndex = 0; DecoratorIndex < NumDecorators; ++DecoratorIndex)
			{
				int32 NumLatentHandles = 0;
				*this << NumLatentHandles;

				FLatentPropertyHandle* LatentHandles = DecoratorTemplates[DecoratorIndex].GetDecoratorLatentPropertyHandles(*NodeDesc);
				for (int32 LatentHandleIndex = 0; LatentHandleIndex < NumLatentHandles; ++LatentHandleIndex)
				{
					*this << LatentHandles[LatentHandleIndex];
				}
			}
		}

		return EErrorState::None;
	}

	FDecoratorReader::EErrorState FDecoratorReader::ReadTrackedObjectsForGC(TArray<TObjectPtr<UObject>>& TrackedObjectsForGC)
	{
		int32 NumTrackedObjectsForGC = 0;
		*this << NumTrackedObjectsForGC;

		TrackedObjectsForGC.SetNumZeroed(NumTrackedObjectsForGC);

		for (int32 TrackedIndex = 0; TrackedIndex < NumTrackedObjectsForGC; ++TrackedIndex)
		{
			// Load the path name to the object
			FString LoadedString;
			*this << LoadedString;

			// Look up the object by fully qualified pathname
			UObject* Obj = FindObject<UObject>(nullptr, *LoadedString, false);
			TrackedObjectsForGC[TrackedIndex] = Obj;
		}

		return EErrorState::None;
	}

	FArchive& FDecoratorReader::operator<<(UObject*& Obj)
	{
		// Load the path name to the object
		FString LoadedString;
		*this << LoadedString;

		// Look up the object by fully qualified pathname
		Obj = FindObject<UObject>(nullptr, *LoadedString, false);

		// If we couldn't find it, load it
		if (Obj == nullptr)
		{
			Obj = LoadObject<UObject>(nullptr, *LoadedString);
		}

		return *this;
	}

	FArchive& FDecoratorReader::operator<<(FObjectPtr& Obj)
	{
		return FArchiveUObject::SerializeObjectPtr(*this, Obj);
	}

	FNodeHandle FDecoratorReader::ResolveNodeHandle(FNodeHandle NodeHandle) const
	{
		if (!NodeHandle.IsValid())
		{
			// The node handle is invalid, return it unchanged
			return NodeHandle;
		}

		check(NodeHandle.IsNodeID());
		const FNodeID NodeID = NodeHandle.GetNodeID();
		check(NodeID.IsValid());

		return NodeHandles[NodeID.GetNodeIndex()];
	}

	FAnimNextDecoratorHandle FDecoratorReader::ResolveDecoratorHandle(FAnimNextDecoratorHandle DecoratorHandle) const
	{
		if (!DecoratorHandle.IsValid())
		{
			// The decorator handle is invalid, return it unchanged
			return DecoratorHandle;
		}

		const FNodeHandle NodeHandle = ResolveNodeHandle(DecoratorHandle.GetNodeHandle());
		return FAnimNextDecoratorHandle(NodeHandle, DecoratorHandle.GetDecoratorIndex());
	}

	FAnimNextDecoratorHandle FDecoratorReader::ResolveEntryPointHandle(FAnimNextEntryPointHandle EntryPointHandle) const
	{
		if (!EntryPointHandle.IsValid())
		{
			// The decorator handle is invalid, return an invalid handle
			return FAnimNextDecoratorHandle();
		}

		const FNodeHandle NodeHandle = ResolveNodeHandle(EntryPointHandle.GetNodeHandle());
		return FAnimNextDecoratorHandle(NodeHandle, EntryPointHandle.GetDecoratorIndex());
	}
}
