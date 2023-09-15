// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DecoratorBase/DecoratorMode.h"
#include "DecoratorBase/DecoratorUID.h"
#include "DecoratorBase/DecoratorRegistryHandle.h"
#include "DecoratorBase/LatentPropertyHandle.h"

#include <type_traits>

class FArchive;
struct FAnimNextDecoratorSharedData;

namespace UE::AnimNext
{
	struct FDecoratorInstanceData;
	struct FNodeDescription;
	struct FNodeInstance;

	/**
	 * Decorator Template
	 * A decorator template represents a specific decorator within a FNodeTemplate.
	 * 
	 * @see FNodeTemplate
	 */
	struct FDecoratorTemplate
	{
		// Returns the globally unique identifier for the decorator
		FDecoratorUID GetUID() const noexcept { return FDecoratorUID(UID); }

		// Returns the decorator registry handle
		FDecoratorRegistryHandle GetRegistryHandle() const noexcept { return RegistryHandle; }

		// Returns the decorator mode
		EDecoratorMode GetMode() const noexcept { return static_cast<EDecoratorMode>(Mode); }

		// For base decorators only, returns the number of additive decorators on top in the stack
		uint32 GetNumAdditiveDecorators() const
		{
			check(GetMode() == EDecoratorMode::Base);
			return AdditiveIndexOrNumAdditive;
		}

		// For additive decorators only, returns the decorator index relative to the base decorator on the stack
		uint32 GetAdditiveDecoratorIndex() const
		{
			check(GetMode() == EDecoratorMode::Additive);
			return AdditiveIndexOrNumAdditive;
		}

		// Returns the offset into the shared data where the descriptor begins, relative to the root of the node's shared data
		uint32 GetNodeSharedOffset() const noexcept { return NodeSharedOffset; }

		// Returns the offset into the shared data where the latent property handles begin (or 0 if we have no latent properties), relative to the root of the node's shared data
		uint32 GetNodeSharedLatentPropertyHandlesOffset() const noexcept { return NodeSharedLatentPropertyHandlesOffset; }

		// Returns the offset into the instance data where the descriptor begins, relative to the root of the node's instance data
		uint32 GetNodeInstanceOffset() const noexcept { return NodeInstanceOffset; }

		// Returns whether or not we have latent properties on this decorator
		bool HasLatentProperties() const noexcept { return NodeSharedLatentPropertyHandlesOffset != 0; }

		// Returns a pointer to the specified decorator description on the current node
		FAnimNextDecoratorSharedData* GetDecoratorDescription(FNodeDescription& NodeDescription) const noexcept
		{
			return reinterpret_cast<FAnimNextDecoratorSharedData*>(reinterpret_cast<uint8*>(&NodeDescription) + GetNodeSharedOffset());
		}

		// Returns a pointer to the specified decorator description on the current node
		const FAnimNextDecoratorSharedData* GetDecoratorDescription(const FNodeDescription& NodeDescription) const noexcept
		{
			return reinterpret_cast<const FAnimNextDecoratorSharedData*>(reinterpret_cast<const uint8*>(&NodeDescription) + GetNodeSharedOffset());
		}

		// Returns a pointer to the specified decorator latent property handles on the current node
		FLatentPropertyHandle* GetDecoratorLatentPropertyHandles(FNodeDescription& NodeDescription) const noexcept
		{
			return reinterpret_cast<FLatentPropertyHandle*>(reinterpret_cast<uint8*>(&NodeDescription) + GetNodeSharedLatentPropertyHandlesOffset());
		}

		// Returns a pointer to the specified decorator latent property handles on the current node
		const FLatentPropertyHandle* GetDecoratorLatentPropertyHandles(const FNodeDescription& NodeDescription) const noexcept
		{
			return reinterpret_cast<const FLatentPropertyHandle*>(reinterpret_cast<const uint8*>(&NodeDescription) + GetNodeSharedLatentPropertyHandlesOffset());
		}

		// Returns a pointer to the specified decorator instance on the current node
		FDecoratorInstanceData* GetDecoratorInstance(FNodeInstance& NodeInstance) const noexcept
		{
			return reinterpret_cast<FDecoratorInstanceData*>(reinterpret_cast<uint8*>(&NodeInstance) + GetNodeInstanceOffset());
		}

		// Returns a pointer to the specified decorator instance on the current node
		const FDecoratorInstanceData* GetDecoratorInstance(const FNodeInstance& NodeInstance) const noexcept
		{
			return reinterpret_cast<const FDecoratorInstanceData*>(reinterpret_cast<const uint8*>(&NodeInstance) + GetNodeInstanceOffset());
		}

		// Serializes this decorator template instance
		ANIMNEXT_API void Serialize(FArchive& Ar);

	private:
		friend struct FNodeTemplate;
		friend struct FNodeTemplateBuilder;

		FDecoratorTemplate(FDecoratorUID InUID, FDecoratorRegistryHandle InRegistryHandle, EDecoratorMode InMode, uint32 InAdditiveIndexOrNumAdditive) noexcept
			: UID(InUID.GetUID())
			, RegistryHandle(InRegistryHandle)
			, Mode(static_cast<uint8>(InMode))
			, AdditiveIndexOrNumAdditive(InAdditiveIndexOrNumAdditive)
			, NodeSharedOffset(0)
			, NodeSharedLatentPropertyHandlesOffset(0)
			, NodeInstanceOffset(0)				// For instance data, 0 is an invalid offset since the data follows an instance of FNodeInstance
			, Padding0(0)
		{}

		FDecoratorUIDRaw			UID;			// decorator globally unique identifier

		FDecoratorRegistryHandle	RegistryHandle;	// decorator registry handle
		uint8						Mode;			// the decorator mode (we only need 1 bit)

		// For base decorators, this contains the number of additive decorators on top
		// For additive decorators, this contains its index relative to the base decorator
		// The first additive decorator has index 1, there is no index 0 (we re-purpose it for the base decorator)
		uint8	AdditiveIndexOrNumAdditive;

		// Offsets into the shared read-only and instance data portions of a node
		// These are not serialized, @see FNodeTemplate::Finalize
		// These are relative to the root of the node description and instance data, respectively (max 64 KB per node each)
		uint16	NodeSharedOffset;						// Start of shared data for this decorator
		uint16	NodeSharedLatentPropertyHandlesOffset;	// Start of shared latent property handles for this decorator
		uint16	NodeInstanceOffset;						// Start of instance data for this decorator

		uint16	Padding0;						// Unused for now

		// TODO: We could cache which parent/base decorator handles which interface
		// This would avoid the need to iterate on every decorator to look up a 'Super'. Perhaps only common interfaces could be cached.
	};

	static_assert(std::is_trivially_copyable_v<FDecoratorTemplate>, "FDecoratorTemplate needs to be trivially copyable");
}
