// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "TraitCore/Trait.h"
#include "TraitInterfaces/IDiscreteBlend.h"
#include "TraitInterfaces/ISmoothBlend.h"

#include "BlendInertializer.generated.h"

class UBlendProfile;

USTRUCT(meta = (DisplayName = "Blend Inertializer", Category = "Default"))
struct FAnimNextBlendInertializerTraitSharedData : public FAnimNextTraitSharedData
{
	GENERATED_BODY()

	// Inertialization Blend Time
	UPROPERTY(EditAnywhere, Category = "Default", meta = (Inline))
	float BlendTime = 0.2f;
};

namespace UE::AnimNext
{
	/**
	 * FBlendInertializerTrait
	 * 
	 * A trait that converts a normal smooth blend into an inertializing blend.
	 */
	struct FBlendInertializerTrait : FAdditiveTrait, IDiscreteBlend, ISmoothBlend
	{
		DECLARE_ANIM_TRAIT(FBlendInertializerTrait, 0x7ea0bdee, FAdditiveTrait)

		using FSharedData = FAnimNextBlendInertializerTraitSharedData;
		using FInstanceData = FTrait::FInstanceData;

		// IDiscreteBlend impl
		virtual void OnBlendTransition(FExecutionContext& Context, const TTraitBinding<IDiscreteBlend>& Binding, int32 OldChildIndex, int32 NewChildIndex) const override;

		// ISmoothBlend impl
		virtual float GetBlendTime(const FExecutionContext& Context, const TTraitBinding<ISmoothBlend>& Binding, int32 ChildIndex) const override;
	};
}
