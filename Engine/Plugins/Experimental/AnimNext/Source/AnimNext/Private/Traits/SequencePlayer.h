// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TraitCore/Trait.h"
#include "TraitInterfaces/IEvaluate.h"
#include "TraitInterfaces/ITimeline.h"
#include "TraitInterfaces/IUpdate.h"
#include "Animation/AnimSequence.h"

#include "SequencePlayer.generated.h"

USTRUCT(meta = (DisplayName = "Sequence Player"))
struct FAnimNextSequencePlayerTraitSharedData : public FAnimNextTraitSharedData
{
	GENERATED_BODY()

	/** The sequence to play. */
	UPROPERTY(EditAnywhere, Category = "Default", meta = (Inline))
	TObjectPtr<UAnimSequence> AnimSequence;

	/** The play rate multiplier at which this sequence plays. */
	UPROPERTY(EditAnywhere, Category = "Default")
	float PlayRate = 1.0f;

	/** The time at which we should start playing this sequence. */
	UPROPERTY(EditAnywhere, Category = "Default")
	float StartPosition = 0.0f;

	/** Whether or not this sequence playback will loop. */
	UPROPERTY(EditAnywhere, Category = "Default")
	bool bLoop = 0.0f;

	// Latent pin support boilerplate
	#define TRAIT_LATENT_PROPERTIES_ENUMERATOR(GeneratorMacro) \
		GeneratorMacro(PlayRate) \
		GeneratorMacro(StartPosition) \
		GeneratorMacro(bLoop) \

	GENERATE_TRAIT_LATENT_PROPERTIES(FAnimNextSequencePlayerTraitSharedData, TRAIT_LATENT_PROPERTIES_ENUMERATOR)
	#undef TRAIT_LATENT_PROPERTIES_ENUMERATOR
};

namespace UE::AnimNext
{
	/**
	 * FSequencePlayerTrait
	 * 
	 * A trait that can play an animation sequence.
	 */
	struct FSequencePlayerTrait : FBaseTrait, IEvaluate, ITimeline, IUpdate
	{
		DECLARE_ANIM_TRAIT(FSequencePlayerTrait, 0x7a0dc157, FBaseTrait)

		using FSharedData = FAnimNextSequencePlayerTraitSharedData;

		struct FInstanceData : FTrait::FInstanceData
		{
			float InternalTimeAccumulator = 0.0f;

			void Construct(const FExecutionContext& Context, const FTraitBinding& Binding);
		};

		// IEvaluate impl
		virtual void PreEvaluate(FEvaluateTraversalContext& Context, const TTraitBinding<IEvaluate>& Binding) const override;

		// ITimeline impl
		virtual float GetPlayRate(const FExecutionContext& Context, const TTraitBinding<ITimeline>& Binding) const override;
		virtual float AdvanceBy(const FExecutionContext& Context, const TTraitBinding<ITimeline>& Binding, float DeltaTime) const override;
		virtual void AdvanceToRatio(const FExecutionContext& Context, const TTraitBinding<ITimeline>& Binding, float ProgressRatio) const override;

		// IUpdate impl
		virtual void PreUpdate(FUpdateTraversalContext& Context, const TTraitBinding<IUpdate>& Binding, const FTraitUpdateState& TraitState) const override;
	};
}
