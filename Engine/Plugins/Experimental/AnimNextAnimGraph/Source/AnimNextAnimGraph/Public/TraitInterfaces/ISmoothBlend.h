// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TraitCore/ITraitInterface.h"
#include "TraitCore/TraitBinding.h"

namespace UE::AnimNext
{
	/**
	 * ISmoothBlend
	 *
	 * This interface exposes blend smoothing related information.
	 */
	struct ANIMNEXTANIMGRAPH_API ISmoothBlend : ITraitInterface
	{
		DECLARE_ANIM_TRAIT_INTERFACE(ISmoothBlend, SMB, 0x1c2c1739)

		// Returns the desired blend time for the specified child
		virtual float GetBlendTime(FExecutionContext& Context, const TTraitBinding<ISmoothBlend>& Binding, int32 ChildIndex) const;
	};

	/**
	 * Specialization for trait binding.
	 */
	template<>
	struct TTraitBinding<ISmoothBlend> : FTraitBinding
	{
		// @see ISmoothBlend::GetBlendTime
		float GetBlendTime(FExecutionContext& Context, int32 ChildIndex) const
		{
			return GetInterface()->GetBlendTime(Context, *this, ChildIndex);
		}

	protected:
		const ISmoothBlend* GetInterface() const { return GetInterfaceTyped<ISmoothBlend>(); }
	};
}
