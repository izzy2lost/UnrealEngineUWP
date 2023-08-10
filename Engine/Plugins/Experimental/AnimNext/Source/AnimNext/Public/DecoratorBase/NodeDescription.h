// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DecoratorBase/NodeID.h"
#include "DecoratorBase/NodeTemplateRegistryHandle.h"

class FArchive;

namespace UE::AnimNext
{
	/**
	 * Node Description
	 * A node description represents a unique instance in the authored static graph.
	 * A node description may have any number of runtime instances in the dynamically executed graph.
	 * As such, a node description is read-only at runtime while a node instance is read/write.
	 * 
	 * A node description is followed in memory by the decorator descriptions (their shared read-only data)
	 * that live within it. Decorator descriptions include things like hard-coded/inline properties, pin links, etc.
	 * 
	 * A node description is itself an instance of a node template.
	 * 
	 * @see FNodeTemplate
	 */
	struct alignas(alignof(uint32)) FNodeDescription
	{
		// Largest allowed size for a node description and the shared data of its decorators
		// We use unsigned 16 bit offsets within the NodeTemplate/DecoratorTemplate
		static constexpr uint32 MAXIMUM_NODE_SHARED_DATA_SIZE = 64 * 1024;

		FNodeDescription(FNodeID InNodeID, FNodeTemplateRegistryHandle InTemplateHandle)
			: NodeID(InNodeID)
			, TemplateHandle(InTemplateHandle)
		{}

		// Returns the node UID, unique to the owning sub-graph
		FNodeID GetUID() const { return NodeID; }

		// Returns the handle of the node's template in the node template registry
		FNodeTemplateRegistryHandle GetTemplateHandle() const { return TemplateHandle; }

		// Serializes this node description instance and the shared data of each decorator that follows
		ANIMNEXT_API void Serialize(FArchive& Ar);

	private:
		FNodeID							NodeID;				// assigned during export/cook, unique to current sub-graph
		FNodeTemplateRegistryHandle		TemplateHandle;		// offset of the node template within the global list

		// Followed by a list of [FAnimNextDecoratorSharedData] instances and optional padding
	};
}
