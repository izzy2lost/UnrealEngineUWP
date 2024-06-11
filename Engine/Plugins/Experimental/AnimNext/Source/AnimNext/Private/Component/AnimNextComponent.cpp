// Copyright Epic Games, Inc. All Rights Reserved.

#include "Component/AnimNextComponent.h"

#include "Blueprint/BlueprintExceptionInfo.h"
#include "Scheduler/Scheduler.h"
#include "Scheduler/ScheduleContext.h"
#include "Param/PropertyBagProxy.h"
#include "Scheduler/ScheduleTaskContext.h"
#include "Scheduler/ScheduleInitializationContext.h"
#include "IAnimNextModuleInterface.h"

namespace UE::AnimNext::Private
{

template<typename ContextType>
static void SetValuesInScopeHelper(const ContextType& InContext, FName InId, FName InScope, EAnimNextParameterScopeOrdering InOrdering, TConstArrayView<FPropertyBagPropertyDesc> InPropertyDescs, TConstArrayView<TConstArrayView<uint8>> InValues)
{
	InContext.ApplyParametersToScope(InScope, (EParameterScopeOrdering)InOrdering, InId, InPropertyDescs, InValues);
}

}

void UAnimNextComponent::OnRegister()
{
	using namespace UE::AnimNext;

	Super::OnRegister();

	if (Schedule)
	{
		// Initialization callback to set up any persistent external parameters
		auto Initialize = [this](const FScheduleInitializationContext& InContext)
		{
			// First group params into scopes
			TMap<FName, TArray<UAnimNextComponentParameter*, TInlineAllocator<4>>, TInlineSetAllocator<4>> ParamsByScope;
			for(UAnimNextComponentParameter* Parameter : Parameters)
			{
				if(Parameter && Parameter->IsValid())
				{
					ParamsByScope.FindOrAdd(Parameter->Scope).Add(Parameter);
				}
			}

			// Now apply to each scope
			for(const TPair<FName, TArray<UAnimNextComponentParameter*, TInlineAllocator<4>>>& ParamPair : ParamsByScope)
			{
				TArray<FPropertyBagPropertyDesc, TInlineAllocator<16>> PropertyDescs;
				TArray<TConstArrayView<uint8>, TInlineAllocator<16>> Values;
				PropertyDescs.Reserve(ParamPair.Value.Num());
				Values.Reserve(ParamPair.Value.Num());
				for(UAnimNextComponentParameter* Parameter : ParamPair.Value)
				{
					FName Name;
					const FProperty* Property;
					Parameter->GetParamInfo(Name, Property);
					PropertyDescs.Emplace(Name, Property);
					Values.Emplace(Property->ContainerPtrToValuePtr<uint8>(Parameter), Property->GetSize());
				}

				// NOTE: Layer is always applied 'before' currently. If we have a use case for 'After' we can add it to UAnimNextComponentParameter
				Private::SetValuesInScopeHelper(InContext, "ComponentParams", ParamPair.Key, EAnimNextParameterScopeOrdering::Before, PropertyDescs, Values);
			}
		};

		check(!SchedulerHandle.IsValid());
		SchedulerHandle = FScheduler::AcquireHandle(this, Schedule, InitMethod, MoveTemp(Initialize));
	}
}

void UAnimNextComponent::OnUnregister()
{
	using namespace UE::AnimNext;

	Super::OnUnregister();

	FScheduler::ReleaseHandle(this, SchedulerHandle);
}

void UAnimNextComponent::SetParameterInScope(FName Scope, EAnimNextParameterScopeOrdering Ordering, FName Name, int32 Value)
{
	checkNoEntry();
}

DEFINE_FUNCTION(UAnimNextComponent::execSetParameterInScope)
{
	using namespace UE::AnimNext;

	// Read wildcard Value input.
	Stack.MostRecentPropertyAddress = nullptr;
	Stack.MostRecentPropertyContainer = nullptr;

	P_GET_PROPERTY(FNameProperty, Scope);
	P_GET_ENUM(EAnimNextParameterScopeOrdering, Ordering);
	P_GET_PROPERTY(FNameProperty, Name);

	Stack.StepCompiledIn<FProperty>(nullptr);
	const FProperty* ValueProp = CastField<FProperty>(Stack.MostRecentProperty);
	const void* ContainerPtr = Stack.MostRecentPropertyContainer;

	P_FINISH;

	if (!ValueProp || !ContainerPtr)
	{
		FBlueprintExceptionInfo ExceptionInfo(
			EBlueprintExceptionType::AbortExecution,
			NSLOCTEXT("AnimNextComponent", "AnimNextComponent_SetParameterInScopeError", "Failed to resolve the Value for Set Parameter In Scope")
		);

		FBlueprintCoreDelegates::ThrowScriptException(P_THIS, Stack, ExceptionInfo);
		return;
	}

	if (Name == NAME_None)
	{
		FBlueprintExceptionInfo ExceptionInfo(
			EBlueprintExceptionType::NonFatalError,
			NSLOCTEXT("AnimNextComponent", "AnimNextComponent_SetParameterInScopeWarning", "Invalid parameter name supplied to Set Parameter In Scope")
		);

		FBlueprintCoreDelegates::ThrowScriptException(P_THIS, Stack, ExceptionInfo);
		return;
	}

	P_NATIVE_BEGIN;

	FInstancedPropertyBag PropertyBag;
	PropertyBag.AddProperty(Name, ValueProp);
	const FProperty* NewProperty = PropertyBag.GetPropertyBagStruct()->GetPropertyDescs()[0].CachedProperty;
	const void* ValuePtr = ValueProp->ContainerPtrToValuePtr<void>(ContainerPtr);
	NewProperty->SetValue_InContainer(PropertyBag.GetMutableValue().GetMemory(), ValuePtr);

	FScheduler::QueueTask(P_THIS, P_THIS->SchedulerHandle, Scope, [Scope, Name, PropertyBag = MoveTemp(PropertyBag), Ordering](const FScheduleTaskContext& InContext) mutable
	{
		InContext.ApplyParametersToScope(Scope, (EParameterScopeOrdering)Ordering, Name, MoveTemp(PropertyBag));
	});

	P_NATIVE_END;
}

void UAnimNextComponent::Enable(bool bEnabled)
{
	UE::AnimNext::FScheduler::EnableHandle(this, SchedulerHandle, bEnabled);
}

void UAnimNextComponent::QueueInputTraitEvent(FAnimNextTraitEventPtr Event)
{
	using namespace UE::AnimNext;

	FScheduler::QueueTask(this, SchedulerHandle, NAME_None, [Event = MoveTemp(Event)](const FScheduleTaskContext& InContext)
		{
			InContext.QueueInputTraitEvent(Event);
		});
}
