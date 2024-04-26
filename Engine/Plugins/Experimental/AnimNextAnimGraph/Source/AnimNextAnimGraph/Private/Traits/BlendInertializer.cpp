// Copyright Epic Games, Inc. All Rights Reserved.

#include "Traits/BlendInertializer.h"

#include "TraitCore/ExecutionContext.h"

#include "Traits/Inertialization.h"


namespace UE::AnimNext
{
	AUTO_REGISTER_ANIM_TRAIT(FBlendInertializerTrait)

	// Trait implementation boilerplate
	#define TRAIT_INTERFACE_ENUMERATOR(GeneratorMacro) \
		GeneratorMacro(IDiscreteBlend) \
		GeneratorMacro(ISmoothBlend) \

	GENERATE_ANIM_TRAIT_IMPLEMENTATION(FBlendInertializerTrait, TRAIT_INTERFACE_ENUMERATOR, NULL_ANIM_TRAIT_EVENT_ENUMERATOR)
	#undef TRAIT_INTERFACE_ENUMERATOR

	void FBlendInertializerTrait::OnBlendTransition(FExecutionContext& Context, const TTraitBinding<IDiscreteBlend>& Binding, int32 OldChildIndex, int32 NewChildIndex) const
	{
		// Trigger the new transition
		IDiscreteBlend::OnBlendTransition(Context, Binding, OldChildIndex, NewChildIndex);

		const FSharedData* SharedData = Binding.GetSharedData<FSharedData>();

		// Make Request Event
		TSharedPtr<FAnimNextInertializationRequestEvent> Event = MakeTraitEvent<FAnimNextInertializationRequestEvent>();
		Event->Request.BlendTime = SharedData->BlendTime;
		Context.RaiseOutputTraitEvent(Event);
	}

	float FBlendInertializerTrait::GetBlendTime(const FExecutionContext& Context, const TTraitBinding<ISmoothBlend>& Binding, int32 ChildIndex) const
	{
		// We hijack the blend time and always transition instantaneously
		return 0.0f;
	}
}
