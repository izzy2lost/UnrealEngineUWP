// Copyright Epic Games, Inc. All Rights Reserved.

#include "Component/AnimNextComponent.h"

#include "Blueprint/BlueprintExceptionInfo.h"
#include "Scheduler/Scheduler.h"
#include "Scheduler/ScheduleContext.h"
#include "Param/PropertyBagProxy.h"
#include "Scheduler/ScheduleTaskContext.h"
#include "Scheduler/ScheduleInitializationContext.h"

namespace UE::AnimNext::Private
{

template<typename ContextType>
static void SetValuesInScopeHelper(const ContextType& InContext, FName InScope, EAnimNextParameterScopeOrdering InOrdering, TConstArrayView<FPropertyBagProxy::FPropertyAndValue> InPropertiesAndValues)
{
	TUniquePtr<FPropertyBagProxy> PropertyBagProxy = MakeUnique<FPropertyBagProxy>();
	PropertyBagProxy->AddPropertiesAndValues(InPropertiesAndValues);
	InContext.ApplyParametersToScope(InScope, (EParameterScopeOrdering)InOrdering, MoveTemp(PropertyBagProxy));
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
				TArray<FPropertyBagProxy::FPropertyAndValue, TInlineAllocator<16>> PropertiesAndValues;
				PropertiesAndValues.Reserve(ParamPair.Value.Num());
				for(UAnimNextComponentParameter* Parameter : ParamPair.Value)
				{
					FPropertyBagProxy::FPropertyAndValue& PropertyAndValue = PropertiesAndValues.AddDefaulted_GetRef();
					PropertyAndValue.ContainerPtr = Parameter;
					Parameter->GetParamInfo(PropertyAndValue.Name, PropertyAndValue.Property);
				}

				// NOTE: Layer is always applied 'before' currently. If we have a use case for 'After' we can add it to UAnimNextComponentParameter
				Private::SetValuesInScopeHelper(InContext, ParamPair.Key, EAnimNextParameterScopeOrdering::Before, PropertiesAndValues);
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

	TUniquePtr<FInstancedPropertyBag> PropertyBag = MakeUnique<FInstancedPropertyBag>();
	PropertyBag->AddProperty(Name, ValueProp);
	const FProperty* NewProperty = PropertyBag->GetPropertyBagStruct()->GetPropertyDescs()[0].CachedProperty;
	const void* ValuePtr = ValueProp->ContainerPtrToValuePtr<void>(ContainerPtr);
	NewProperty->SetValue_InContainer(PropertyBag->GetMutableValue().GetMemory(), ValuePtr);

	FScheduler::QueueTask(P_THIS, P_THIS->SchedulerHandle, Scope, [Scope, Name, NewProperty, PropertyBag = MoveTemp(PropertyBag), Ordering](const FScheduleTaskContext& InContext) mutable
	{
		FPropertyBagProxy::FPropertyAndValue PropertyAndValue;
		PropertyAndValue.Name = Name;
		PropertyAndValue.Property = NewProperty;
		PropertyAndValue.ContainerPtr = PropertyBag->GetValue().GetMemory();
		Private::SetValuesInScopeHelper(InContext, Scope, Ordering, { PropertyAndValue });
	});

	P_NATIVE_END;
}

void UAnimNextComponent::Enable(bool bEnabled)
{
	UE::AnimNext::FScheduler::EnableHandle(this, SchedulerHandle, bEnabled);
}
