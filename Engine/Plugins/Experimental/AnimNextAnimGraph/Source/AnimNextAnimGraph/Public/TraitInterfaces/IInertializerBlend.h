// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TraitCore/ITraitInterface.h"
#include "TraitCore/TraitBinding.h"

namespace UE::AnimNext
{
	/**
	 * IInertializerBlend
	 *
	 * This interface exposes inertializing blend related information.
	 */
	struct ANIMNEXTANIMGRAPH_API IInertializerBlend : ITraitInterface
	{
		DECLARE_ANIM_TRAIT_INTERFACE(IInertializerBlend, IB, 0x3856b8e9)

		// Returns the desired blend time for the specified child
		virtual float GetBlendTime(FExecutionContext& Context, const TTraitBinding<IInertializerBlend>& Binding, int32 ChildIndex) const;
	};

	/**
	 * Specialization for trait binding.
	 */
	template<>
	struct TTraitBinding<IInertializerBlend> : FTraitBinding
	{
		// @see IInertializerBlend::GetBlendTime
		float GetBlendTime(FExecutionContext& Context, int32 ChildIndex) const
		{
			return GetInterface()->GetBlendTime(Context, *this, ChildIndex);
		}

	protected:
		const IInertializerBlend* GetInterface() const { return GetInterfaceTyped<IInertializerBlend>(); }
	};
}
