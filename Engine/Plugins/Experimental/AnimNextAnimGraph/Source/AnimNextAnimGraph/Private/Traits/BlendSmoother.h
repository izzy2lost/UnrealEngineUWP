// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "AlphaBlend.h"
#include "TraitCore/Trait.h"
#include "TraitInterfaces/IDiscreteBlend.h"
#include "TraitInterfaces/IEvaluate.h"
#include "TraitInterfaces/ISmoothBlend.h"
#include "TraitInterfaces/IUpdate.h"

#include "BlendSmoother.generated.h"

class UCurveFloat;

USTRUCT(meta = (DisplayName = "Blend Smoother"))
struct FAnimNextBlendSmootherTraitSharedData : public FAnimNextTraitSharedData
{
	GENERATED_BODY()

	/** How long to take when blending into each child. */
	UPROPERTY(EditAnywhere, Category = "Default", meta = (Inline))
	TArray<float> BlendTimes;

	/** What type of blend equation to use when converting the time elapsed into a blend weight. */
	UPROPERTY(EditAnywhere, Category = "Default", meta = (Inline))
	EAlphaBlendOption BlendType = EAlphaBlendOption::Linear;

	/** Custom curve to use when the Custom blend type is used. */
	UPROPERTY(EditAnywhere, Category = "Default", meta = (Inline))
	TObjectPtr<UCurveFloat> CustomBlendCurve;
};

namespace UE::AnimNext
{
	/**
	 * FBlendSmootherTrait
	 * 
	 * A trait that smoothly blends between discrete states over time.
	 */
	struct FBlendSmootherTrait : FAdditiveTrait, IEvaluate, IUpdate, IDiscreteBlend, ISmoothBlend
	{
		DECLARE_ANIM_TRAIT(FBlendSmootherTrait, 0xbaeb537b, FAdditiveTrait)

		using FSharedData = FAnimNextBlendSmootherTraitSharedData;

		// Struct for tracking blends for each pose
		struct FBlendData
		{
			// Helper struct to update a time based weight
			FAlphaBlend Blend;

			// Current child weight (normalized with all children)
			float Weight = 0.0f;

			// Whether or not this child is actively blending
			bool bIsBlending = false;
		};

		struct FInstanceData : FTrait::FInstanceData
		{
			// Blend state per child
			TArray<FBlendData> PerChildBlendData;
		};

		// IEvaluate impl
		virtual void PostEvaluate(FEvaluateTraversalContext& Context, const TTraitBinding<IEvaluate>& Binding) const override;

		// IUpdate impl
		virtual void PreUpdate(FUpdateTraversalContext& Context, const TTraitBinding<IUpdate>& Binding, const FTraitUpdateState& TraitState) const override;

		// IDiscreteBlend impl
		virtual float GetBlendWeight(FExecutionContext& Context, const TTraitBinding<IDiscreteBlend>& Binding, int32 ChildIndex) const override;
		virtual const FAlphaBlend* GetBlendState(FExecutionContext& Context, const TTraitBinding<IDiscreteBlend>& Binding, int32 ChildIndex) const override;
		virtual void OnBlendTransition(FExecutionContext& Context, const TTraitBinding<IDiscreteBlend>& Binding, int32 OldChildIndex, int32 NewChildIndex) const override;

		// ISmoothBlend impl
		virtual float GetBlendTime(FExecutionContext& Context, const TTraitBinding<ISmoothBlend>& Binding, int32 ChildIndex) const override;

		// Internal impl
		static void InitializeInstanceData(FExecutionContext& Context, const FTraitBinding& Binding, const FSharedData* SharedData, FInstanceData* InstanceData);
	};
}
