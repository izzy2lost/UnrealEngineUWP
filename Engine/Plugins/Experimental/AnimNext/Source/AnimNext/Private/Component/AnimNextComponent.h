// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "Param/AnimNextParameterCollection.h"
#include "Param/IAnimNextParameterSourceInterface.h"
#include "Scheduler/AnimNextSchedule.h"
#include "Scheduler/ScheduleHandle.h"
#include "AnimNextComponent.generated.h"

class UAnimNextSchedule;
struct FAnimNextComponentInstanceData;

UCLASS(MinimalAPI, Blueprintable, meta = (BlueprintSpawnableComponent))
class UAnimNextComponent : public UActorComponent, public IAnimNextParameterSourceInterface
{
	GENERATED_BODY()

	friend struct FAnimNextComponentInstanceData;

	// UActorComponent interface
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	virtual TStructOnScope<FActorComponentInstanceData> GetComponentInstanceData() const override;

	// IAnimNextParameterSourceInterface interface
	virtual void UpdateLayer(UE::AnimNext::FParamStackLayerHandle& InHandle) const override;
	virtual UE::AnimNext::FParamStackLayerHandle CacheLayer() const override;

	// Because UHT says: "A Private function cannot be a BlueprintImplementableEvent!"
protected:
	// Override point to allow self to be updated
	UFUNCTION(Category = "AnimNext", BlueprintImplementableEvent, meta=(BlueprintThreadSafe, ForceAsFunction))
	void Update();

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
	UAnimNextSchedule* Schedule = nullptr;

	// Parameters to apply at each scope
	UPROPERTY(EditAnywhere, Category = "Parameters", meta = (CustomWidget = "ParamName", AllowedParamType = "FAnimNextScope"))
	TMap<FName, FAnimNextParameterCollection> Parameters;

	// How to initialize the schedule
	UPROPERTY(EditAnywhere, Category="Schedule")
	EAnimNextScheduleInitMethod InitMethod = EAnimNextScheduleInitMethod::InitializeAndPauseInEditor;

	// Handle to the registered results/schedule
	UE::AnimNext::FScheduleHandle SchedulerHandle;

	// Cached layer handle for 'self'
	mutable UE::AnimNext::FParamStackLayerHandle LayerHandle;
};

/** Used to store component data during RerunConstructionScripts */
USTRUCT()
struct FAnimNextComponentInstanceData : public FActorComponentInstanceData
{
	GENERATED_BODY()

	FAnimNextComponentInstanceData() = default;
	FAnimNextComponentInstanceData(const UAnimNextComponent* InSourceComponent)
		: FActorComponentInstanceData(InSourceComponent)
	{}

private:
	friend class UAnimNextComponent;

	virtual bool ContainsData() const override
	{
		return true;
	}

	virtual void ApplyToComponent(UActorComponent* Component, const ECacheApplyPhase CacheApplyPhase) override;

	UPROPERTY(EditAnywhere, Category = "Schedule")
	UAnimNextSchedule* Schedule = nullptr;

	UPROPERTY(EditAnywhere, Category = "Parameters")
	TMap<FName, FAnimNextParameterCollection> Parameters;
};

