// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/MemStack.h"
#include "TraitCore/ITraitInterface.h"

namespace UE::AnimNext
{
	// An array of children pointers
	// We reserve a small amount inline and spill on the memstack
	using FChildrenArray = TArray<FWeakTraitPtr, TInlineAllocator<8, TMemStackAllocator<>>>;

	/**
	 * IHierarchy
	 * 
	 * This interface exposes hierarchy traversal information to navigate the graph.
	 */
	struct ANIMNEXT_API IHierarchy : ITraitInterface
	{
		DECLARE_ANIM_TRAIT_INTERFACE(IHierarchy, 0x846d8a37)

		// Returns the number of children
		// Includes inactive children
		virtual uint32 GetNumChildren(const FExecutionContext& Context, const TTraitBinding<IHierarchy>& Binding) const;

		// Appends weak handles to any children we wish to traverse.
		// Traits are responsible for allocating and releasing child instance data.
		// Empty handles and duplicates can be appended.
		virtual void GetChildren(const FExecutionContext& Context, const TTraitBinding<IHierarchy>& Binding, FChildrenArray& Children) const;
	};

	/**
	 * Specialization for trait binding.
	 */
	template<>
	struct TTraitBinding<IHierarchy> : FTraitBinding
	{
		// @see IHierarchy::GetNumChildren
		uint32 GetNumChildren(const FExecutionContext& Context) const
		{
			return GetInterface()->GetNumChildren(Context, *this);
		}

		// @see IHierarchy::GetChildren
		void GetChildren(const FExecutionContext& Context, FChildrenArray& Children) const
		{
			GetInterface()->GetChildren(Context, *this, Children);
		}

	protected:
		const IHierarchy* GetInterface() const { return GetInterfaceTyped<IHierarchy>(); }
	};
}
