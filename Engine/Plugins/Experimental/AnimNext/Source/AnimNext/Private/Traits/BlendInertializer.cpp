// Copyright Epic Games, Inc. All Rights Reserved.

#include "Traits/BlendInertializer.h"

#include "TraitCore/ExecutionContext.h"

namespace UE::AnimNext
{
	AUTO_REGISTER_ANIM_TRAIT(FBlendInertializerTrait)

	// Trait implementation boilerplate
	#define TRAIT_INTERFACE_ENUMERATOR(GeneratorMacro) \
		GeneratorMacro(IDiscreteBlend) \
		GeneratorMacro(ISmoothBlend) \

	GENERATE_ANIM_TRAIT_IMPLEMENTATION(FBlendInertializerTrait, TRAIT_INTERFACE_ENUMERATOR)
	#undef TRAIT_INTERFACE_ENUMERATOR

	void FBlendInertializerTrait::OnBlendTransition(const FExecutionContext& Context, const TTraitBinding<IDiscreteBlend>& Binding, int32 OldChildIndex, int32 NewChildIndex) const
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

	float FBlendInertializerTrait::GetBlendTime(const FExecutionContext& Context, const TTraitBinding<ISmoothBlend>& Binding, int32 ChildIndex) const
	{
		// We hijack the blend time and always transition instantaneously
		return 0.0f;
	}
}
