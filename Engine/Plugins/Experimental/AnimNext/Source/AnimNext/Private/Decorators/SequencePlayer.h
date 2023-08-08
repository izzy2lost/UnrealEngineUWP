// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DecoratorBase/Decorator.h"
#include "DecoratorInterfaces/IEvaluate.h"
#include "DecoratorInterfaces/ITimeline.h"
#include "DecoratorInterfaces/IUpdate.h"
#include "Animation/AnimSequence.h"

#include "SequencePlayer.generated.h"

USTRUCT()
struct FAnimNextSequencePlayerDecoratorSharedData : public FAnimNextDecoratorSharedData
{
	GENERATED_BODY()

	/** The sequence to play. */
	UPROPERTY(meta = (Input))
	TObjectPtr<UAnimSequence> AnimSequence;

	/** The play rate multiplier at which this sequence plays. */
	UPROPERTY(meta = (Input))
	float PlayRate = 1.0f;

	/** The time at which we should start playing this sequence. */
	UPROPERTY(meta = (Input))
	float StartPosition = 0.0f;

	/** Whether or not this sequence playback will loop. */
	UPROPERTY(meta = (Input))
	bool bLoop = 0.0f;
};

namespace UE::AnimNext
{
	/**
	 * FSequencePlayerDecorator
	 * 
	 * A decorator that can play an animation sequence.
	 */
	struct FSequencePlayerDecorator : FBaseDecorator, IEvaluate, ITimeline, IUpdate
	{
		DECLARE_ANIM_DECORATOR(FSequencePlayerDecorator, 0xa628ad12, FBaseDecorator)

		using FSharedData = FAnimNextSequencePlayerDecoratorSharedData;

		struct FInstanceData : FDecorator::FInstanceData
		{
			float InternalTimeAccumulator = 0.0f;
			float PrevInternalTimeAccumulator = 0.0f;

			void Construct(FExecutionContext& Context, FWeakDecoratorPtr DecoratorPtr, const FAnimNextSequencePlayerDecoratorSharedData& SharedData);
		};

		// IEvaluate impl
		virtual void PreEvaluate(FExecutionContext& Context, const TDecoratorBinding<IEvaluate>& Binding) const override;

		// ITimeline impl
		virtual double GetPlayRate(FExecutionContext& Context, const TDecoratorBinding<ITimeline>& Binding) const override;

		// IUpdate impl
		virtual void PreUpdate(FExecutionContext& Context, const TDecoratorBinding<IUpdate>& Binding) const override;
	};
}
