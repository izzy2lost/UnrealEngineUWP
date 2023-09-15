// Copyright Epic Games, Inc. All Rights Reserved.

#include "DecoratorBase/NodeTemplate.h"

#include "Serialization/Archive.h"
#include "DecoratorBase/Decorator.h"
#include "DecoratorBase/DecoratorRegistry.h"
#include "DecoratorBase/NodeDescription.h"
#include "DecoratorBase/NodeInstance.h"

namespace UE::AnimNext
{
	void FNodeTemplate::Serialize(FArchive& Ar)
	{
		Ar << UID;
		Ar << NumDecorators;

		const uint32 NumDecorators_ = GetNumDecorators();
		FDecoratorTemplate* DecoratorTemplates = GetDecorators();

		for (uint32 DecoratorIndex = 0; DecoratorIndex < NumDecorators_; ++DecoratorIndex)
		{
			DecoratorTemplates[DecoratorIndex].Serialize(Ar);
		}

		if (Ar.IsLoading())
		{
			// When loading, make sure to recompute all runtime dependent values (e.g. sizes and offsets)
			Finalize();
		}
	}

	void FNodeTemplate::Finalize()
	{
		const FDecoratorRegistry& DecoratorRegistry = FDecoratorRegistry::Get();

		const uint32 NumDecorators_ = GetNumDecorators();
		FDecoratorTemplate* DecoratorTemplates = GetDecorators();

		uint32 SharedDataOffset = sizeof(FNodeDescription);
		uint32 InstanceDataOffset = sizeof(FNodeInstance);

		for (uint32 DecoratorIndex = 0; DecoratorIndex < NumDecorators_; ++DecoratorIndex)
		{
			FDecoratorTemplate& DecoratorTemplate = DecoratorTemplates[DecoratorIndex];
			const FDecoratorUID DecoratorUID = DecoratorTemplate.GetUID();

			uint32 DecoratorSharedDataOffset = 0;
			uint32 DecoratorSharedLatentPropertyHandlesOffset = 0;
			uint32 DecoratorInstanceDataOffset = 0;	// For instance data, 0 is an invalid offset since the data follows an instance of FNodeInstance

			// Skip decorators that we can't find
			// If a decorator isn't loaded and we attempt to run the graph, it will be a no-op entry
			if (const FDecorator* Decorator = DecoratorRegistry.Find(DecoratorUID))
			{
				const FDecoratorMemoryLayout MemoryLayout = Decorator->GetDecoratorMemoryDescription();

				// Align our data
				SharedDataOffset = Align(SharedDataOffset, MemoryLayout.SharedDataAlignment);
				InstanceDataOffset = Align(InstanceDataOffset, MemoryLayout.InstanceDataAlignment);

				// Save our decorator offsets
				DecoratorSharedDataOffset = SharedDataOffset;
				DecoratorInstanceDataOffset = InstanceDataOffset;

				// Include our decorator
				SharedDataOffset += MemoryLayout.SharedDataSize;
				InstanceDataOffset += MemoryLayout.InstanceDataSize;

				// Save our latent pins offset (if we have any)
				const uint32 NumLatentProperties = Decorator->GetNumLatentDecoratorProperties();
				DecoratorSharedLatentPropertyHandlesOffset = NumLatentProperties != 0 ? Align(SharedDataOffset, alignof(FLatentPropertyHandle)) : 0;

				// Include our latent pins
				SharedDataOffset += NumLatentProperties * sizeof(FLatentPropertyHandle);
			}

			check(DecoratorSharedDataOffset <= MAX_uint16);
			check(DecoratorSharedLatentPropertyHandlesOffset <= MAX_uint16);
			check(DecoratorInstanceDataOffset <= MAX_uint16);

			// Update our decorator offsets
			DecoratorTemplate.NodeSharedOffset = static_cast<uint16>(DecoratorSharedDataOffset);
			DecoratorTemplate.NodeSharedLatentPropertyHandlesOffset = static_cast<uint16>(DecoratorSharedLatentPropertyHandlesOffset);
			DecoratorTemplate.NodeInstanceOffset = static_cast<uint16>(DecoratorInstanceDataOffset);
		}

		// Our size is the offset of the decorator that would follow afterwards
		// If the size is too large, we'll end up truncating the offsets/size
		// Set a value of 0 to be able to detect it later
		NodeSharedDataSize = SharedDataOffset > MAX_uint16 ? 0 : static_cast<uint16>(SharedDataOffset);
		NodeInstanceDataSize = InstanceDataOffset > MAX_uint16 ? 0 : static_cast<uint16>(InstanceDataOffset);
	}
}
