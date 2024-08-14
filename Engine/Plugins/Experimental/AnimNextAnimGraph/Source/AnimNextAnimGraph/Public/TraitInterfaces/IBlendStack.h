// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TraitCore/ITraitInterface.h"
#include "TraitCore/TraitBinding.h"
#include "AlphaBlend.h"
#include "Graph/AnimNextAnimationGraph.h"
#include "Graph/AnimNextGraphInstancePtr.h"

namespace UE::AnimNext
{
	/**
	 * IBlendStack
	 *
	 * This interface exposes anything needed to request a new subgraph
	 */
	struct ANIMNEXTANIMGRAPH_API IBlendStack : ITraitInterface
	{
		DECLARE_ANIM_TRAIT_INTERFACE(IBlendStack, 0xf3468e64)

		struct FGraphRequest
		{
			/**
			 * TODO: Add more blend options here as we need them.
			 * Consider making dynamic payload if we want to implement a special blend framework.
			 */

			// Blend-in duration for this graph request 
			FAlphaBlendArgs BlendArgs;

			// The template graph to use for the new graph instance
			TObjectPtr<const UAnimNextAnimationGraph> AnimationGraph;
		};


		// Returns the desired blend time for the specified child
		virtual void PushGraph(FExecutionContext& Context, const TTraitBinding<IBlendStack>& Binding, const IBlendStack::FGraphRequest& GraphRequest, FAnimNextGraphInstancePtr& OutGraphInstance) const;
		virtual void GetActiveGraphRequest(FExecutionContext& Context, const TTraitBinding<IBlendStack>& Binding, IBlendStack::FGraphRequest& OutRequest) const;

#if WITH_EDITOR
		virtual const FText& GetDisplayName() const override;
		virtual const FText& GetDisplayShortName() const override;
#endif // WITH_EDITOR
	};

	/**
	 * Specialization for trait binding.
	 */
	template<>
	struct TTraitBinding<IBlendStack> : FTraitBinding
	{
		// @see IBlendStack::PushGraph
		void PushGraph(FExecutionContext& Context, const IBlendStack::FGraphRequest& GraphRequest, FAnimNextGraphInstancePtr& OutGraphInstance) const
		{
			GetInterface()->PushGraph(Context, *this, GraphRequest, OutGraphInstance);
		}

		// @see IBlendStack::GetActiveGraphRequest
		void GetActiveGraphRequest(FExecutionContext& Context, IBlendStack::FGraphRequest& OutRequest) const
		{
			GetInterface()->GetActiveGraphRequest(Context, *this, OutRequest);
		}

	protected:
		const IBlendStack* GetInterface() const { return GetInterfaceTyped<IBlendStack>(); }
	};
}
