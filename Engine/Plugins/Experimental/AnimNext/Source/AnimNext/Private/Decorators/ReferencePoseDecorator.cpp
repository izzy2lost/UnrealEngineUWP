// Copyright Epic Games, Inc. All Rights Reserved.

#include "Decorators/ReferencePoseDecorator.h"

#include "DecoratorBase/ExecutionContext.h"
#include "EvaluationVM/Tasks/PushReferenceKeyframe.h"

namespace UE::AnimNext
{
	AUTO_REGISTER_ANIM_DECORATOR(FReferencePoseDecorator)

	DEFINE_ANIM_DECORATOR_BEGIN(FReferencePoseDecorator)
		DEFINE_ANIM_DECORATOR_IMPLEMENTS_INTERFACE(IEvaluate)
	DEFINE_ANIM_DECORATOR_END(FReferencePoseDecorator)

	void FReferencePoseDecorator::PreEvaluate(const FExecutionContext& Context, const TDecoratorBinding<IEvaluate>& Binding) const
	{
		const FSharedData* SharedData = Binding.GetSharedData<FSharedData>();

		FAnimNextPushReferenceKeyframeTask Task;
		Task.bIsAdditive = SharedData->ReferencePoseType == EAnimNextReferencePoseType::AdditiveIdentity;

		FEvaluateTraversalContext& TraversalContext = Context.GetTraversalContext<FEvaluateTraversalContext>();
		TraversalContext.AppendTask(Task);
	}
}
