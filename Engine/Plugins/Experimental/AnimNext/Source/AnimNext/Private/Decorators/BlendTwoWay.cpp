// Copyright Epic Games, Inc. All Rights Reserved.

#include "Decorators/BlendTwoWay.h"

#include "DecoratorBase/ExecutionContext.h"

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
		// TODO:
		// Now that our children have finished evaluating, we can pop their two poses and blend it
		// Children execute in depth first order, as such the poses need to be popped in reverse order
		// FPose&& pose1 = Context.PopPose();
		// FPose&& pose0 = Context.PopPose();
		// FPose resultPose = BlendTwoWay(pose0, pose1, blendWeight);
		// Context.PushPose(resultPose);
		// If we have full weight on a child, there is no work to do and we can early exit instead
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
