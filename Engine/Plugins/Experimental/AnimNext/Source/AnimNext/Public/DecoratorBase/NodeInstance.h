// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DecoratorBase/DecoratorTemplate.h"
#include "DecoratorBase/NodeHandle.h"

namespace UE::AnimNext
{
	/**
	 * Node Instance
	 * A node instance represents allocated data for specific node.
	 *
	 * In order to access the decorator instance data, the offsets need to be looked up in the node template.
	 * 
	 * @see FNodeTemplate, FDecoratorTemplate
	 */
	struct ANIMNEXT_API FNodeInstance
	{
		// Largest allowed size for a node instance and the instance data of its decorators
		// We use unsigned 16 bit offsets within the NodeTemplate/DecoratorTemplate
		static constexpr uint32 MAXIMUM_NODE_INSTANCE_DATA_SIZE = 64 * 1024;

		FNodeInstance(const FNodeInstance&) = delete;
		FNodeInstance& operator=(const FNodeInstance&) = delete;

		// Returns whether the node instance is valid or not
		bool IsValid() const { return NodeHandle.IsValid(); }

		// Returns a handle to the shared data for this node
		FNodeHandle GetNodeHandle() const { return NodeHandle; }

		// Returns the number of live references to this node instance, does not include weak handles
		uint32 GetReferenceCount() const { return ReferenceCount; }

	private:
		explicit FNodeInstance(FNodeHandle NodeHandle);
		~FNodeInstance() { check(ReferenceCount == 0); }

		void AddReference() { ReferenceCount++; }
		void ReleaseReference();

		uint32		ReferenceCount;		// how many non-weak FDecoratorPtr handles point to us, not thread safe
		FNodeHandle	NodeHandle;			// relative to root of sub-graph, should this be a pointer?

		// Followed by a list of [FDecoratorInstanceData] instances and optional padding

		friend struct FDecoratorPtr;
		friend struct FExecutionContext;
		friend struct FNodePtr;
	};
}
