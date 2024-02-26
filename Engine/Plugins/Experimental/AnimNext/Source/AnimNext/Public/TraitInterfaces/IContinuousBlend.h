// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TraitCore/ITraitInterface.h"
#include "TraitCore/TraitBinding.h"

namespace UE::AnimNext
{
	/**
	 * IContinuousBlend
	 *
	 * This interface exposes continuous blend related information.
	 */
	struct ANIMNEXT_API IContinuousBlend : ITraitInterface
	{
		DECLARE_ANIM_TRAIT_INTERFACE(IContinuousBlend, 0xe7d79186)

		// Returns the blend weight for the specified child
		// Multiple children can have non-zero weight but their sum must be 1.0
		// Returns -1.0 if the child index is invalid
		virtual float GetBlendWeight(const FExecutionContext& Context, const TTraitBinding<IContinuousBlend>& Binding, int32 ChildIndex) const;
	};

	/**
	 * Specialization for trait binding.
	 */
	template<>
	struct TTraitBinding<IContinuousBlend> : FTraitBinding
	{
		// @see IContinuousBlend::GetBlendWeight
		float GetBlendWeight(const FExecutionContext& Context, int32 ChildIndex) const
		{
			return GetInterface()->GetBlendWeight(Context, *this, ChildIndex);
		}

	protected:
		const IContinuousBlend* GetInterface() const { return GetInterfaceTyped<IContinuousBlend>(); }
	};
}
