// Copyright Epic Games, Inc. All Rights Reserved.

#include "Decorators/BlendTwoWay.h"

#include "DecoratorBase/ExecutionContext.h"
#include "EvaluationVM/Tasks/BlendKeyframes.h"

namespace UE::AnimNext
{
	AUTO_REGISTER_ANIM_DECORATOR(FBlendTwoWayDecorator)

	DEFINE_ANIM_DECORATOR_BEGIN(FBlendTwoWayDecorator)
		DEFINE_ANIM_DECORATOR_IMPLEMENTS_INTERFACE(IEvaluate)
		DEFINE_ANIM_DECORATOR_IMPLEMENTS_INTERFACE(IUpdate)
		DEFINE_ANIM_DECORATOR_IMPLEMENTS_INTERFACE(IHierarchy)
	DEFINE_ANIM_DECORATOR_END(FBlendTwoWayDecorator)

	void FBlendTwoWayDecorator::PostEvaluate(FExecutionContext& Context, const TDecoratorBinding<IEvaluate>& Binding) const
	{
		const FInstanceData* InstanceData = Binding.GetInstanceData<FInstanceData>();

		if (InstanceData->ChildA.IsValid() && InstanceData->ChildB.IsValid())
		{
			// We have two children, interpolate them
			const FSharedData* SharedData = Binding.GetSharedData<FSharedData>();

			const float BlendWeight = SharedData->GetBlendWeight(Context, Binding);

			FEvaluateTraversalContext& TraversalContext = Context.GetTraversalContext<FEvaluateTraversalContext>();
			TraversalContext.AppendTask(FAnimNextBlendTwoKeyframesTask::Make(BlendWeight));
		}
		else
		{
			// We have only one child that is active, do nothing
		}
	}

	void FBlendTwoWayDecorator::PreUpdate(FExecutionContext& Context, const TDecoratorBinding<IUpdate>& Binding) const
	{
		const FSharedData* SharedData = Binding.GetSharedData<FSharedData>();
		FInstanceData* InstanceData = Binding.GetInstanceData<FInstanceData>();

		const float BlendWeight = SharedData->GetBlendWeight(Context, Binding);
		if (BlendWeight < 1.0f)
		{
			if (!InstanceData->ChildA.IsValid())
			{
				// We need to blend a child that isn't instanced yet, allocate it
				InstanceData->ChildA = Context.AllocateNodeInstance(Binding, SharedData->ChildA);
			}

			if (BlendWeight == 0.0f)
			{
				// We no longer need this child, release it
				InstanceData->ChildB.Reset();
			}
		}

		if (BlendWeight > 0.0f)
		{
			if (!InstanceData->ChildB.IsValid())
			{
				// We need to blend a child that isn't instanced yet, allocate it
				InstanceData->ChildB = Context.AllocateNodeInstance(Binding, SharedData->ChildB);
			}

			if (BlendWeight == 1.0f)
			{
				// We no longer need this child, release it
				InstanceData->ChildA.Reset();
			}
		}
	}

	void FBlendTwoWayDecorator::GetChildren(FExecutionContext& Context, const TDecoratorBinding<IHierarchy>& Binding, FChildrenArray& Children) const
	{
		const FInstanceData* InstanceData = Binding.GetInstanceData<FInstanceData>();

		// Add the two child handles, even if they are empty
		Children.Add(InstanceData->ChildA);
		Children.Add(InstanceData->ChildB);
	}
}
