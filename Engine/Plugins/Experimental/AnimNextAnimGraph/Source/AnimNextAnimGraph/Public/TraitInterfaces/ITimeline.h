// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TraitCore/ITraitInterface.h"
#include "TraitCore/TraitBinding.h"

namespace UE::AnimNext
{
	/**
	 * ITimeline
	 *
	 * This interface exposes timeline related information.
	 */
	struct ANIMNEXTANIMGRAPH_API ITimeline : ITraitInterface
	{
		DECLARE_ANIM_TRAIT_INTERFACE(ITimeline, TIM, 0x53760727)

		// Returns the play rate of this timeline
		virtual float GetPlayRate(FExecutionContext& Context, const TTraitBinding<ITimeline>& Binding) const;

		// Advances time by the provided delta time (positive or negative) on this timeline
		// Returns the progress of playback
		virtual float AdvanceBy(FExecutionContext& Context, const TTraitBinding<ITimeline>& Binding, float DeltaTime) const;

		// Advances time to the specified progress ratio on this timeline
		// Progress ratio must be between [0.0, 1.0]
		virtual void AdvanceToRatio(FExecutionContext& Context, const TTraitBinding<ITimeline>& Binding, float ProgressRatio) const;
	};

	/**
	 * Specialization for trait binding.
	 */
	template<>
	struct TTraitBinding<ITimeline> : FTraitBinding
	{
		// @see ITimeline::GetPlayRate
		float GetPlayRate(FExecutionContext& Context) const
		{
			return GetInterface()->GetPlayRate(Context, *this);
		}

		// @see ITimeline::AdvanceBy
		float AdvanceBy(FExecutionContext& Context, float DeltaTime) const
		{
			return GetInterface()->AdvanceBy(Context, *this, DeltaTime);
		}

		// @see ITimeline::AdvanceToRatio
		void AdvanceToRatio(FExecutionContext& Context, float ProgressRatio) const
		{
			GetInterface()->AdvanceToRatio(Context, *this, ProgressRatio);
		}

	protected:
		const ITimeline* GetInterface() const { return GetInterfaceTyped<ITimeline>(); }
	};
}
