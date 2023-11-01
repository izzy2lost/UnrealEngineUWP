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
		// Initialization callback to set up any persistent external parameters
		auto Initialize = [this](const FScheduleContext& InContext)
		{
			FScheduleInstanceData& InstanceData = InContext.GetInstanceData();

			// First group params into scopes
			TMap<FName, TArray<UAnimNextComponentParameter*, TInlineAllocator<4>>, TInlineSetAllocator<4>> ParamsByScope;
			for(UAnimNextComponentParameter* Parameter : Parameters)
			{
				if(Parameter && Parameter->IsValid())
				{
					ParamsByScope.FindOrAdd(Parameter->Scope).Add(Parameter);
				}
			}

			for(const TPair<FName, TArray<UAnimNextComponentParameter*, TInlineAllocator<4>>>& ParamPair : ParamsByScope)
			{
				FName Scope = ParamPair.Key;
				TSharedPtr<FParamStack> StackToUse;

				if(Scope == NAME_None)
				{
					StackToUse = InstanceData.RootParamStack;
				}
				else
				{
					const FAnimNextScheduleParamScopeEntryTask* FoundTask = InContext.Schedule->ParamScopeEntryTasks.FindByPredicate([Scope](const FAnimNextScheduleParamScopeEntryTask& InTask)
					{
						return InTask.Scope == Scope;
					});

					if(FoundTask)
					{
						StackToUse = InstanceData.ParamStacks[FoundTask->ParamScopeIndex];
					}
				}

				if(StackToUse.IsValid())
				{
					TArray<TPair<FParamId, Private::FParamEntry>, TInlineAllocator<4>> Params;
					for(UAnimNextComponentParameter* Parameter : ParamPair.Value)
					{
						FParamId ParamId;
						FAnimNextParamType Type;
						uint8* Value = nullptr;
						Parameter->GetParamInfo(ParamId, Type, Value);
						check(Type.IsValid() && Value != nullptr);

						constexpr bool bIsReference = true;
						constexpr bool bIsMutable = false;
						Params.Emplace(ParamId, Private::FParamEntry(Type.GetHandle(), TArrayView<uint8>(Value, 1), bIsReference, bIsMutable));
					}

					FParamStackLayerHandle NewLayer = FParamStack::MakeLayer(Params);
					InstanceData.StaticUserHandles.Add(MoveTemp(NewLayer));

					// Layer is never popped as this is happening at the very start of execution
					StackToUse->PushLayer(InstanceData.StaticUserHandles.Last());
				}
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
	SchedulerHandle.Invalidate();
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
