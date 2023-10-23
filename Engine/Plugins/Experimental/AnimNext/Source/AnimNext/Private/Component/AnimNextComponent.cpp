// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNextComponent.h"
#include "Scheduler/Scheduler.h"
#include "Scheduler/ScheduleContext.h"
#include "Param/AnimNextParameterSourceRef.h"
#include "Param/Params.h"

void UAnimNextComponent::OnRegister()
{
	using namespace UE::AnimNext;

	Super::OnRegister();

	if (Schedule)
	{
		check(!SchedulerHandle.IsValid());
		SchedulerHandle = FScheduler::AcquireHandle(this, Schedule, Parameters, InitMethod);
	}
}

void UAnimNextComponent::OnUnregister()
{
	using namespace UE::AnimNext;

	Super::OnUnregister();

	FScheduler::ReleaseHandle(this, SchedulerHandle);
	SchedulerHandle.Invalidate();
}

void UAnimNextComponent::UpdateLayer(UE::AnimNext::FParamStackLayerHandle& InHandle) const
{
	if (UAnimNextComponent* This = InHandle.As<UAnimNextComponent>())
	{
		This->Update();
	}
}

UE::AnimNext::FParamStackLayerHandle UAnimNextComponent::CacheLayer() const
{
	return UE::AnimNext::FParamStack::MakeReferenceLayer(const_cast<UAnimNextComponent*>(this));
}

TStructOnScope<FActorComponentInstanceData> UAnimNextComponent::GetComponentInstanceData() const
{
	TStructOnScope<FActorComponentInstanceData> InstanceData = MakeStructOnScope<FActorComponentInstanceData, FAnimNextComponentInstanceData>(this);
	FAnimNextComponentInstanceData* AnimNextComponentInstanceData = InstanceData.Cast<FAnimNextComponentInstanceData>();
	AnimNextComponentInstanceData->Schedule = Schedule;
	AnimNextComponentInstanceData->Parameters = Parameters;

	return InstanceData;
}

void UAnimNextComponent::SetParameterInScope(FName Scope, FName Name, int32 Value)
{
	checkNoEntry();
}

DEFINE_FUNCTION(UAnimNextComponent::execSetParameterInScope)
{
	using namespace UE::AnimNext;

	// Read wildcard Value input.
	Stack.MostRecentPropertyAddress = nullptr;
	Stack.MostRecentPropertyContainer = nullptr;

	PARAM_PASSED_BY_VAL(Scope, FNameProperty, FName);
	PARAM_PASSED_BY_VAL(Name, FNameProperty, FName);

	Stack.StepCompiledIn<FProperty>(nullptr);
	const FProperty* ValueProp = CastField<FProperty>(Stack.MostRecentProperty);
	const void* ValuePtr = Stack.MostRecentPropertyAddress;

	P_FINISH;

	if (!ValueProp || !ValuePtr)
	{
		FBlueprintExceptionInfo ExceptionInfo(
			EBlueprintExceptionType::AbortExecution,
			NSLOCTEXT("AnimNextComponent", "AnimNextComponent_SetParameterInScopeError", "Failed to resolve the Value for Set Parameter In Scope")
		);

		FBlueprintCoreDelegates::ThrowScriptException(P_THIS, Stack, ExceptionInfo);
	}
	else if (Scope == NAME_None || Name == NAME_None)
	{
		FBlueprintExceptionInfo ExceptionInfo(
			EBlueprintExceptionType::NonFatalError,
			NSLOCTEXT("AnimNextComponent", "AnimNextComponent_SetParameterInScopeWarning", "Invalid scope or parameter name supplied to Set Parameter In Scope")
		);

		FBlueprintCoreDelegates::ThrowScriptException(P_THIS, Stack, ExceptionInfo);
	}
	else
	{
		P_NATIVE_BEGIN;

		FAnimNextParameterSourceRef ParamSource;
		ParamSource.Type = EAnimNextParameterSourceRefType::Inline;
		ParamSource.InlineParameters.AddProperty(Name, ValueProp);
		ParamSource.InlineParameters.SetValue(Name, ParamSource.InlineParameters.GetPropertyBagStruct()->FindPropertyByName(Name), ValuePtr);

		FScheduler::QueueTask(P_THIS, P_THIS->SchedulerHandle, Scope, [Scope, ParamSource = MoveTemp(ParamSource)](const FScheduleContext& InContext)
		{
			FAnimNextParameterCollection& Collection = InContext.GetInstanceData().UserScopes.FindOrAdd(Scope);

			// TODO: pre/post scope distinction
			Collection.Parameters.Insert(ParamSource, 0);
		});
		
		P_NATIVE_END;
	}
}

void UAnimNextComponent::Enable(bool bEnabled)
{
	UE::AnimNext::FScheduler::EnableHandle(this, SchedulerHandle, bEnabled);
}

void FAnimNextComponentInstanceData::ApplyToComponent(UActorComponent* Component, const ECacheApplyPhase CacheApplyPhase) 
{
	Super::ApplyToComponent(Component, CacheApplyPhase);

	UAnimNextComponent* AnimNextComponent = CastChecked<UAnimNextComponent>(Component);
	AnimNextComponent->Schedule = Schedule;
	AnimNextComponent->Parameters = Parameters;
}
