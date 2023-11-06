// Copyright Epic Games, Inc. All Rights Reserved.

#include "Decorators/BlendInertializer.h"

#include "DecoratorBase/ExecutionContext.h"

namespace UE::AnimNext
{
	AUTO_REGISTER_ANIM_DECORATOR(FBlendInertializerDecorator)

	DEFINE_ANIM_DECORATOR_BEGIN(FBlendInertializerDecorator)
		DEFINE_ANIM_DECORATOR_IMPLEMENTS_INTERFACE(IDiscreteBlend)
		DEFINE_ANIM_DECORATOR_IMPLEMENTS_INTERFACE(ISmoothBlend)
	DEFINE_ANIM_DECORATOR_END(FBlendInertializerDecorator)

	void FBlendInertializerDecorator::OnBlendTransition(const FExecutionContext& Context, const TDecoratorBinding<IDiscreteBlend>& Binding, int32 OldChildIndex, int32 NewChildIndex) const
	{
		// Trigger the new transition
		IDiscreteBlend::OnBlendTransition(Context, Binding, OldChildIndex, NewChildIndex);

		// TODO: Implement the inertialization request API
#if 0
		UE::Anim::IInertializationRequester* InertializationRequester = Context.GetMessage<UE::Anim::IInertializationRequester>();
		if (InertializationRequester)
		{
			FInertializationRequest Request;
			Request.Duration = CurrentBlendTimes[ChildIndex];		// TODO: Get from ISmoothBlend interface using super
			Request.BlendProfile = CurrentBlendProfile;				// TODO: Get from shared data
			Request.bUseBlendMode = true;
			Request.BlendMode = GetBlendType();						// TODO: Add to ISmoothBlend interface
			Request.CustomBlendCurve = GetCustomBlendCurve();		// TODO: Add to ISmoothBlend interface

			InertializationRequester->RequestInertialization(Request);
			bRequestedInertializationOnActiveChildIndexChange = true;
		}
		else
		{
			FAnimNode_Inertialization::LogRequestError(Context, BlendPose[ChildIndex]);
		}
#endif
	}

	float FBlendInertializerDecorator::GetBlendTime(const FExecutionContext& Context, const TDecoratorBinding<ISmoothBlend>& Binding, int32 ChildIndex) const
	{
		// We hijack the blend time and always transition instantaneously
		return 0.0f;
	}
}
