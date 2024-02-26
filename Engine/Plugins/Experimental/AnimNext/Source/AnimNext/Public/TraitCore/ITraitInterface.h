// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TraitCore/TraitInterfaceUID.h"

// Helper macros
#define DECLARE_ANIM_TRAIT_INTERFACE(InterfaceName, InterfaceNameHash) \
	/* Globally unique UID for this interface */ \
	static constexpr UE::AnimNext::FTraitInterfaceUID InterfaceUID = UE::AnimNext::FTraitInterfaceUID(InterfaceNameHash, TEXT(#InterfaceName)); \
	virtual UE::AnimNext::FTraitInterfaceUID GetInterfaceUID() const override { return InterfaceUID; }

namespace UE::AnimNext
{
	struct FExecutionContext;	// Derived types will have functions that accept the execution context

	/**
	 * ITraitInterface
	 * 
	 * Base type for all trait interfaces. Used for type safety.
	 */
	struct ANIMNEXT_API ITraitInterface
	{
		virtual ~ITraitInterface() {}

		// The globally unique UID for this interface
		// Derived types will have their own InterfaceUID member that hides/aliases/shadows this one
		// @see DECLARE_ANIM_TRAIT_INTERFACE
		static constexpr FTraitInterfaceUID InterfaceUID = FTraitInterfaceUID(0xcdc27733, TEXT("ITraitInterface"));

		// Returns the globally unique UID for this interface
		virtual FTraitInterfaceUID GetInterfaceUID() const { return InterfaceUID; };
	};
}
