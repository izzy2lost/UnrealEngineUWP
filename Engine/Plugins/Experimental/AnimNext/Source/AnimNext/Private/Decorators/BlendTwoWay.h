// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "DecoratorBase/Decorator.h"
#include "DecoratorInterfaces/IEvaluate.h"
#include "DecoratorInterfaces/IHierarchy.h"
#include "DecoratorInterfaces/IUpdate.h"

#include "BlendTwoWay.generated.h"

USTRUCT()
struct FAnimNextBlendTwoWayDecoratorSharedData : public FAnimNextDecoratorSharedData
{
	GENERATED_BODY()

	/** First output to be blended (full weight is 0.0). */
	UPROPERTY()
	FAnimNextDecoratorHandle ChildA;

	/** Second output to be blended (full weight is 1.0). */
	UPROPERTY()
	FAnimNextDecoratorHandle ChildB;

	/** How much to blend our two children: 0.0 is fully child A while 1.0 is fully child B. */
	UPROPERTY()
	float BlendWeight = 0.0f;

	// Latent pin support boilerplate
	#define DECORATOR_LATENT_PROPERTIES_ENUMERATOR(GeneratorMacro) \
		GeneratorMacro(BlendWeight) \

	GENERATE_DECORATOR_LATENT_PROPERTIES(FAnimNextBlendTwoWayDecoratorSharedData, DECORATOR_LATENT_PROPERTIES_ENUMERATOR)
	#undef DECORATOR_LATENT_PROPERTIES_ENUMERATOR
};

namespace UE::AnimNext
{
	/**
	 * FBlendTwoWayDecorator
	 * 
	 * A decorator that can blend two inputs.
	 */
	struct FBlendTwoWayDecorator : FBaseDecorator, IEvaluate, IUpdate, IHierarchy
	{
		DECLARE_ANIM_DECORATOR(FBlendTwoWayDecorator, 0x96a81d1e, FBaseDecorator)

		using FSharedData = FAnimNextBlendTwoWayDecoratorSharedData;

		struct FInstanceData : FDecorator::FInstanceData
		{
			FDecoratorPtr ChildA;
			FDecoratorPtr ChildB;
		};

		// IEvaluate impl
		virtual void PostEvaluate(FExecutionContext& Context, const TDecoratorBinding<IEvaluate>& Binding) const override;

		// IUpdate impl
		virtual void PreUpdate(FExecutionContext& Context, const TDecoratorBinding<IUpdate>& Binding) const override;

		// IHierarchy impl
		virtual void GetChildren(FExecutionContext& Context, const TDecoratorBinding<IHierarchy>& Binding, FChildrenArray& Children) const override;
	};
}
