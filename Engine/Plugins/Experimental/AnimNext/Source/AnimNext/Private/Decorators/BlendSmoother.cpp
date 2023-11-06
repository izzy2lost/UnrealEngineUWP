// Copyright Epic Games, Inc. All Rights Reserved.

#include "Decorators/BlendSmoother.h"

#include "Animation/AnimTypes.h"
#include "DecoratorBase/ExecutionContext.h"
#include "DecoratorInterfaces/IHierarchy.h"
#include "EvaluationVM/Tasks/BlendKeyframes.h"

namespace UE::AnimNext
{
	AUTO_REGISTER_ANIM_DECORATOR(FBlendSmootherDecorator)

	DEFINE_ANIM_DECORATOR_BEGIN(FBlendSmootherDecorator)
		DEFINE_ANIM_DECORATOR_IMPLEMENTS_INTERFACE(IUpdate)
		DEFINE_ANIM_DECORATOR_IMPLEMENTS_INTERFACE(IDiscreteBlend)
		DEFINE_ANIM_DECORATOR_IMPLEMENTS_INTERFACE(ISmoothBlend)
	DEFINE_ANIM_DECORATOR_END(FBlendSmootherDecorator)

	void FBlendSmootherDecorator::PreUpdate(FUpdateTraversalContext& Context, const TDecoratorBinding<IUpdate>& Binding, const FDecoratorUpdateState& DecoratorState) const
	{
		const FSharedData* SharedData = Binding.GetSharedData<FSharedData>();
		FInstanceData* InstanceData = Binding.GetInstanceData<FInstanceData>();

		// If this is our first update, allocate our blend data
		if (InstanceData->PerChildBlendData.IsEmpty())
		{
			InitializeInstanceData(Context, Binding, SharedData, InstanceData);
		}

		// Update the decorators below us, they might trigger a transition
		IUpdate::PreUpdate(Context, Binding, DecoratorState);

		const float DeltaTime = DecoratorState.GetDeltaTime();

		// Advance the weights
		float SumWeight = 0.0f;
		uint32 NumBlending = 0;

		for (FBlendData& ChildBlendData : InstanceData->PerChildBlendData)
		{
			if (!ChildBlendData.bIsBlending)
			{
				continue;	// Skip children that aren't blending
			}

			ChildBlendData.Blend.Update(DeltaTime);

			float NewBlendWeight = ChildBlendData.Blend.GetBlendedValue();

			if (!FAnimWeight::IsRelevant(NewBlendWeight))
			{
				// Our new weight is no longer relevant, snap it to zero and normalization below will fix-up the other weights
				// We'll then terminate the blend below
				NewBlendWeight = 0.0f;
			}

			ChildBlendData.Weight = NewBlendWeight;
			SumWeight += NewBlendWeight;
			NumBlending++;
		}

		if (NumBlending <= 1)
		{
			return;	// Nothing to do if we don't blend at least 2 children together
		}

		// Renormalize the weights if the sum isn't near 0.0 or near 1.0
		if (SumWeight > ZERO_ANIMWEIGHT_THRESH &&
			FMath::Abs(SumWeight - 1.0f) > ZERO_ANIMWEIGHT_THRESH)
		{
			const float ReciprocalSum = 1.0f / SumWeight;

			for (FBlendData& ChildBlendData : InstanceData->PerChildBlendData)
			{
				ChildBlendData.Weight *= ReciprocalSum;
			}
		}

		TDecoratorBinding<IDiscreteBlend> DiscreteBlendDecorator;
		Context.GetInterface(Binding, DiscreteBlendDecorator);

		// Free any newly inactive children
		const int32 NumChildren = InstanceData->PerChildBlendData.Num();
		for (int32 ChildIndex = 0; ChildIndex < NumChildren; ++ChildIndex)
		{
			FBlendData& ChildBlendData = InstanceData->PerChildBlendData[ChildIndex];

			if (ChildBlendData.bIsBlending && ChildBlendData.Weight <= 0.0f)
			{
				// This child has finished blending out, terminate it
				DiscreteBlendDecorator.OnBlendTerminated(Context, ChildIndex);

				ChildBlendData.bIsBlending = false;
			}
		}
	}

	float FBlendSmootherDecorator::GetBlendWeight(const FExecutionContext& Context, const TDecoratorBinding<IDiscreteBlend>& Binding, int32 ChildIndex) const
	{
		const FInstanceData* InstanceData = Binding.GetInstanceData<FInstanceData>();
		return InstanceData->PerChildBlendData.IsValidIndex(ChildIndex) ? InstanceData->PerChildBlendData[ChildIndex].Weight : -1.0f;
	}

	const FAlphaBlend* FBlendSmootherDecorator::GetBlendState(const FExecutionContext& Context, const TDecoratorBinding<IDiscreteBlend>& Binding, int32 ChildIndex) const
	{
		const FInstanceData* InstanceData = Binding.GetInstanceData<FInstanceData>();
		return InstanceData->PerChildBlendData.IsValidIndex(ChildIndex) ? &InstanceData->PerChildBlendData[ChildIndex].Blend : nullptr;
	}

	void FBlendSmootherDecorator::OnBlendTransition(const FExecutionContext& Context, const TDecoratorBinding<IDiscreteBlend>& Binding, int32 OldChildIndex, int32 NewChildIndex) const
	{
		const FSharedData* SharedData = Binding.GetSharedData<FSharedData>();
		FInstanceData* InstanceData = Binding.GetInstanceData<FInstanceData>();

		// scale by the weight difference since we want consistency:
		// - if you're moving from 0 to full weight 1, it will use the normal blend time
		// - if you're moving from 0.5 to full weight 1, it will get there in half the time
		const float NewChildCurrentWeight = InstanceData->PerChildBlendData[NewChildIndex].Weight;
		const float NewChildDesiredWeight = 1.0f;
		const float WeightDifference = FMath::Clamp(FMath::Abs(NewChildDesiredWeight - NewChildCurrentWeight), 0.0f, 1.0f);

		TDecoratorBinding<ISmoothBlend> SmoothBlendDecorator;
		Context.GetInterface(Binding, SmoothBlendDecorator);

		const float BlendTime = SmoothBlendDecorator.GetBlendTime(Context, NewChildIndex);
		const float RemainingBlendTime = OldChildIndex != INDEX_NONE ? (BlendTime * WeightDifference) : 0.0f;

		if (OldChildIndex != INDEX_NONE)
		{
			// Make sure the old child starts blending out
			FBlendData& OldChildBlendData = InstanceData->PerChildBlendData[OldChildIndex];
			OldChildBlendData.Blend.SetValueRange(OldChildBlendData.Weight, 0.0f);
			check(OldChildBlendData.bIsBlending);
		}

		{
			// Setup the new child to blend in
			FBlendData& NewChildBlendData = InstanceData->PerChildBlendData[NewChildIndex];
			NewChildBlendData.Blend.SetValueRange(NewChildBlendData.Weight, 1.0f);
			NewChildBlendData.Blend.ResetAlpha();	// Reset the alpha right away in case another decorator needs it
			NewChildBlendData.bIsBlending = true;
		}

		// We set the new blend time on all children
		for (FBlendData& ChildBlendData : InstanceData->PerChildBlendData)
		{
			ChildBlendData.Blend.SetBlendTime(RemainingBlendTime);
		}

		// Don't call the super since we hijack the transition to smooth it out over time
		// We just initiate the new blend manually

		TDecoratorBinding<IDiscreteBlend> DiscreteBlendDecorator;
		Context.GetInterface(Binding, DiscreteBlendDecorator);

		DiscreteBlendDecorator.OnBlendInitiated(Context, NewChildIndex);
	}

	float FBlendSmootherDecorator::GetBlendTime(const FExecutionContext& Context, const TDecoratorBinding<ISmoothBlend>& Binding, int32 ChildIndex) const
	{
		const FSharedData* SharedData = Binding.GetSharedData<FSharedData>();
		return SharedData->BlendTimes.IsValidIndex(ChildIndex) ? SharedData->BlendTimes[ChildIndex] : 0.0f;
	}

	void FBlendSmootherDecorator::InitializeInstanceData(const FExecutionContext& Context, const FDecoratorBinding& Binding, const FSharedData* SharedData, FInstanceData* InstanceData)
	{
		check(InstanceData->PerChildBlendData.IsEmpty());

		uint32 NumExpectedChildren = SharedData->BlendTimes.Num();

#if DO_CHECK
		TDecoratorBinding<IHierarchy> HierarchyDecorator;
		Context.GetInterface(Binding, HierarchyDecorator);

		const uint32 NumActualChildren = HierarchyDecorator.GetNumChildren(Context);
		ensureMsgf(NumActualChildren != 0, TEXT("BlendSmootherDecorator has %u blend times for %u children"), NumExpectedChildren, NumActualChildren);
		NumExpectedChildren = NumActualChildren;
#endif

		InstanceData->PerChildBlendData.SetNum(NumExpectedChildren);

		for (FBlendData& ChildBlendData : InstanceData->PerChildBlendData)
		{
			ChildBlendData.Blend.SetBlendOption(SharedData->BlendType);
			ChildBlendData.Blend.SetCustomCurve(SharedData->CustomBlendCurve);
		}
	}
}
