// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AnimNextComponentParameter.h"
#include "Components/ActorComponent.h"
#include "Scheduler/AnimNextSchedule.h"
#include "Scheduler/ScheduleHandle.h"
#include "AnimNextComponent.generated.h"

class UAnimNextSchedule;
struct FAnimNextComponentInstanceData;

UCLASS(MinimalAPI, meta = (BlueprintSpawnableComponent))
class UAnimNextComponent : public UActorComponent
{
	GENERATED_BODY()

	// UActorComponent interface
	virtual void OnRegister() override;
	virtual void OnUnregister() override;

public:
	// Sets a parameter in the scope. Scopes correspond to an existing scope in a schedule.
	UFUNCTION(BlueprintCallable, Category = "AnimNext", CustomThunk, meta = (CustomStructureParam = Value, UnsafeDuringActorConstruction))
	void SetParameterInScope(FName Scope, FName Name, int32 Value);

	// Enable or disable this component's update
	UFUNCTION(BlueprintCallable, Category = "AnimNext")
	void Enable(bool bEnabled);
	
private:
	DECLARE_FUNCTION(execSetParameterInScope);

private:
	// The execution schedule that this component will run
	UPROPERTY(EditAnywhere, Category="Schedule")
	TObjectPtr<UAnimNextSchedule> Schedule = nullptr;

	// Parameters to apply on schedule/component registration
	UPROPERTY(Instanced, EditAnywhere, Category = "Parameters")
	TArray<TObjectPtr<UAnimNextComponentParameter>> Parameters;

	// How to initialize the schedule
	UPROPERTY(EditAnywhere, Category="Schedule")
	EAnimNextScheduleInitMethod InitMethod = EAnimNextScheduleInitMethod::InitializeAndPauseInEditor;

	// Handle to the registered results/schedule
	UE::AnimNext::FScheduleHandle SchedulerHandle;
};
