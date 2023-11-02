// Copyright Epic Games, Inc. All Rights Reserved.

#include "Decorators/BlendByBool.h"

#include "DecoratorBase/ExecutionContext.h"
#include "EvaluationVM/Tasks/BlendKeyframes.h"

#if 0
// TODO: Validate blend by bool and blend smoother through a cvar at runtime to select the bool value
static TAutoConsoleVariable<int32> CVarAnimNextForceBlendBool(TEXT("a.AnimNextForceBlendBool"), -1, TEXT("If != -1, then the value [0 (false), 1 (true)] is used to control Blend By Bool and override its value."));
#endif

namespace UE::AnimNext
{
	AUTO_REGISTER_ANIM_DECORATOR(FBlendByBoolDecorator)

	DEFINE_ANIM_DECORATOR_BEGIN(FBlendByBoolDecorator)
		DEFINE_ANIM_DECORATOR_IMPLEMENTS_INTERFACE(IDiscreteBlend)
		DEFINE_ANIM_DECORATOR_IMPLEMENTS_INTERFACE(IEvaluate)
		DEFINE_ANIM_DECORATOR_IMPLEMENTS_INTERFACE(IHierarchy)
		DEFINE_ANIM_DECORATOR_IMPLEMENTS_INTERFACE(IUpdate)
	DEFINE_ANIM_DECORATOR_END(FBlendByBoolDecorator)

	static constexpr int32 TRUE_CHILD_INDEX = 0;
	static constexpr int32 FALSE_CHILD_INDEX = 1;

	void FBlendByBoolDecorator::PostEvaluate(FExecutionContext& Context, const TDecoratorBinding<IEvaluate>& Binding) const
	{
		const FInstanceData* InstanceData = Binding.GetInstanceData<FInstanceData>();

		if (InstanceData->TrueChild.IsValid() && InstanceData->FalseChild.IsValid())
		{
			// We have two children, interpolate them

			TDecoratorBinding<IDiscreteBlend> DiscreteBlendDecorator;
			Context.GetInterface(Binding, DiscreteBlendDecorator);

			const float BlendWeight = DiscreteBlendDecorator.GetBlendWeight(Context, FALSE_CHILD_INDEX);

			FEvaluateTraversalContext& TraversalContext = Context.GetTraversalContext<FEvaluateTraversalContext>();
			TraversalContext.AppendTask(FAnimNextBlendTwoKeyframesTask::Make(BlendWeight));
		}
		else
		{
			// We have only one child that is active, do nothing
		}
	}

	void FBlendByBoolDecorator::PreUpdate(FExecutionContext& Context, const TDecoratorBinding<IUpdate>& Binding) const
	{
		const FSharedData* SharedData = Binding.GetSharedData<FSharedData>();
		FInstanceData* InstanceData = Binding.GetInstanceData<FInstanceData>();

		TDecoratorBinding<IDiscreteBlend> DiscreteBlendDecorator;
		Context.GetInterface(Binding, DiscreteBlendDecorator);

		const float DestinationChildIndex = DiscreteBlendDecorator.GetBlendDestinationChildIndex(Context);
		if (InstanceData->PreviousChildIndex != DestinationChildIndex)
		{
			DiscreteBlendDecorator.OnBlendTransition(Context, InstanceData->PreviousChildIndex, DestinationChildIndex);

			InstanceData->PreviousChildIndex = DestinationChildIndex;
		}
	}

	uint32 FBlendByBoolDecorator::GetNumChildren(FExecutionContext& Context, const TDecoratorBinding<IHierarchy>& Binding) const
	{
		return 2;
	}

	void FBlendByBoolDecorator::GetChildren(FExecutionContext& Context, const TDecoratorBinding<IHierarchy>& Binding, FChildrenArray& Children) const
	{
		const FInstanceData* InstanceData = Binding.GetInstanceData<FInstanceData>();

		// Add the two children, even if the handles are empty
		Children.Add(InstanceData->TrueChild);
		Children.Add(InstanceData->FalseChild);
	}

	float FBlendByBoolDecorator::GetBlendWeight(FExecutionContext& Context, const TDecoratorBinding<IDiscreteBlend>& Binding, int32 ChildIndex) const
	{
		TDecoratorBinding<IDiscreteBlend> DiscreteBlendDecorator;
		Context.GetInterface(Binding, DiscreteBlendDecorator);

		const float DestinationChildIndex = DiscreteBlendDecorator.GetBlendDestinationChildIndex(Context);

		if (ChildIndex == TRUE_CHILD_INDEX)
		{
			return (DestinationChildIndex == TRUE_CHILD_INDEX) ? 1.0f : 0.0f;
		}
		else if (ChildIndex == FALSE_CHILD_INDEX)
		{
			return (DestinationChildIndex == FALSE_CHILD_INDEX) ? 1.0f : 0.0f;
		}
		else
		{
			// Invalid child index
			return -1.0f;
		}
	}

	int32 FBlendByBoolDecorator::GetBlendDestinationChildIndex(FExecutionContext& Context, const TDecoratorBinding<IDiscreteBlend>& Binding) const
	{
		const FSharedData* SharedData = Binding.GetSharedData<FSharedData>();

#if 0
		{
			const int32 BoolOverride = CVarAnimNextForceBlendBool.GetValueOnAnyThread();
			if (BoolOverride == 0)
			{
				return FALSE_CHILD_INDEX;
			}
			else if (BoolOverride == 1)
			{
				return TRUE_CHILD_INDEX;
			}
		}
#endif

		const bool bCondition = SharedData->GetbCondition(Context, Binding);
		return bCondition ? TRUE_CHILD_INDEX : FALSE_CHILD_INDEX;
	}

	void FBlendByBoolDecorator::OnBlendTransition(FExecutionContext& Context, const TDecoratorBinding<IDiscreteBlend>& Binding, int32 OldChildIndex, int32 NewChildIndex) const
	{
		TDecoratorBinding<IDiscreteBlend> DiscreteBlendDecorator;
		Context.GetInterface(Binding, DiscreteBlendDecorator);

		// We initiate immediately when we transition
		DiscreteBlendDecorator.OnBlendInitiated(Context, NewChildIndex);

		// We terminate immediately when we transition
		DiscreteBlendDecorator.OnBlendTerminated(Context, OldChildIndex);
	}

	void FBlendByBoolDecorator::OnBlendInitiated(FExecutionContext& Context, const TDecoratorBinding<IDiscreteBlend>& Binding, int32 ChildIndex) const
	{
		const FSharedData* SharedData = Binding.GetSharedData<FSharedData>();
		FInstanceData* InstanceData = Binding.GetInstanceData<FInstanceData>();

		// Allocate our new child instance
		if (ChildIndex == TRUE_CHILD_INDEX)
		{
			if (!InstanceData->TrueChild.IsValid())
			{
				InstanceData->TrueChild = Context.AllocateNodeInstance(Binding, SharedData->TrueChild);
			}
		}
		else if (ChildIndex == FALSE_CHILD_INDEX)
		{
			if (!InstanceData->FalseChild.IsValid())
			{
				InstanceData->FalseChild = Context.AllocateNodeInstance(Binding, SharedData->FalseChild);
			}
		}
	}

	void FBlendByBoolDecorator::OnBlendTerminated(FExecutionContext& Context, const TDecoratorBinding<IDiscreteBlend>& Binding, int32 ChildIndex) const
	{
		FInstanceData* InstanceData = Binding.GetInstanceData<FInstanceData>();

		// Deallocate our child instance
		if (ChildIndex == TRUE_CHILD_INDEX)
		{
			InstanceData->TrueChild.Reset();
		}
		else if (ChildIndex == FALSE_CHILD_INDEX)
		{
			InstanceData->FalseChild.Reset();
		}
	}
}
