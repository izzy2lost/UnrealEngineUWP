// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AnimNextScheduleGraphTask.h"
#include "Scheduler/AnimNextSchedulePortTask.h"
#include "Scheduler/AnimNextScheduleExternalTask.h"
#include "Scheduler/AnimNextScheduleParamScopeTask.h"
#include "Tasks/Task.h"
#include "AnimNextSchedule.generated.h"

class UAnimNextGraph;
class UAnimNextParameterBlock;
class UAnimNextSchedule;
class UAnimNextSchedulerWorldSubsystem;
class UAnimNextComponent;
struct FAnimNextSchedulerEntry;

namespace UE::AnimNext
{
	struct FScheduler;
	struct FSchedulerImpl;
	struct FScheduleResult;
	struct FScheduleContext;
	struct FScheduleInstanceData;
	struct FScheduleTickFunction;
}

UCLASS(EditInlineNew, Abstract)
class UAnimNextScheduleEntry : public UObject
{
	GENERATED_BODY()

private:
	friend class UAnimNextSchedule;
};

UCLASS()
class UAnimNextScheduleEntry_AnimNextGraph : public UAnimNextScheduleEntry
{
	GENERATED_BODY()

private:
	friend class UAnimNextSchedule;

	UPROPERTY(EditAnywhere, Category = "Graph", meta = (CustomWidget = "ParamName", AllowedParamType = "FAnimNextScope"))
	FName Name;

	UPROPERTY(EditAnywhere, Category = "Graph", meta = (CustomWidget = "ParamName", AllowedParamType = "FName"))
	FName EntryPoint;

	UPROPERTY(EditAnywhere, Category = "Graph")
	TObjectPtr<UAnimNextGraph> Graph;

	UPROPERTY(EditAnywhere, Category = "Graph")
	TArray<TObjectPtr<UAnimNextParameterBlock>> ParameterBlocks;
};

UCLASS()
class UAnimNextScheduleEntry_Port : public UAnimNextScheduleEntry
{
	GENERATED_BODY()

private:
	friend class UAnimNextSchedule;

	UPROPERTY(EditAnywhere, Category = "Port", meta = (CustomWidget = "ParamName", AllowedParamType = "FAnimNextPort"))
	FName Name;
};

UCLASS()
class UAnimNextScheduleEntry_ExternalTask : public UAnimNextScheduleEntry
{
	GENERATED_BODY()

private:
	friend class UAnimNextSchedule;

	UPROPERTY(EditAnywhere, Category = "External Task", meta = (CustomWidget = "ParamName", AllowedParamType = "FTickFunction"))
	FName Name;

	UPROPERTY(EditAnywhere, Category = "External Task", meta = (CustomWidget = "ParamName", AllowedParamType = "FName"))
	FName ObjectName;
};

UCLASS()
class UAnimNextScheduleEntry_ParamScope : public UAnimNextScheduleEntry
{
	GENERATED_BODY()

private:
	friend class UAnimNextSchedule;

	UPROPERTY(EditAnywhere, Category = "Parameters", meta = (CustomWidget = "ParamName", AllowedParamType = "FAnimNextScope"))
	FName Name;

	// Parameters to apply in this scope
	UPROPERTY(EditAnywhere, Category = "Parameters")
	TArray<TObjectPtr<UAnimNextParameterBlock>> ParameterBlocks;

	// Entries that are part of this scope
	UPROPERTY(EditAnywhere, Category = "Parameters", Instanced)
	TArray<TObjectPtr<UAnimNextScheduleEntry>> SubEntries;
};

// TEMP: opcode in schedule
UENUM()
enum class EAnimNextScheduleScheduleOpcode : uint8
{
	None,
	RunTask,				// Operand = Task Index
	BeginRunExternalTask,	// Operand = Task Index
	EndRunExternalTask,		// Operand = Task Index
	RunPort,				// Operand = Port Index
	RunParamScopeEntry,		// Operand = Scope Index
	RunParamScopeExit,		// Operand = Scope Index
	PrerequisiteTask,		// Operand = Task Index
	PrerequisiteBeginExternalTask, // Operand = Task Index
	PrerequisiteEndExternalTask, // Operand = Task Index
	PrerequisiteScopeEntry,	// Operand = Scope Index
	PrerequisiteScopeExit,	// Operand = Scope Index
	Exit,					// Operand = 0
};

// TEMP: Bytecode instruction in schedule
USTRUCT()
struct FAnimNextScheduleInstruction
{
	GENERATED_BODY()

	UPROPERTY()
	EAnimNextScheduleScheduleOpcode Opcode;

	UPROPERTY()
	int32 Operand;
};

UCLASS()
class ANIMNEXT_API UAnimNextSchedule : public UObject
{
	GENERATED_BODY()

private:
	friend struct UE::AnimNext::FScheduler;
	friend struct UE::AnimNext::FSchedulerImpl;
	friend struct UE::AnimNext::FScheduleContext;
	friend struct UE::AnimNext::FScheduleInstanceData;
	friend struct UE::AnimNext::FScheduleTickFunction;
	friend struct FAnimNextSchedulerEntry;
	friend class UAnimNextComponent;
	friend class UAnimNextSchedulerWorldSubsystem;

	// UObject interface
	virtual void PostLoad() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

	// Compile the editor data into a compact runtime representation
	void CompileSchedule();
#endif

#if WITH_EDITORONLY_DATA
	// Editor only
	// TODO: move this into an editor only subobject
	// TODO: this is currently only a linear list, we want it to be a graph
	UPROPERTY(EditAnywhere, Category = "Schedule", Instanced)
	TArray<TObjectPtr<UAnimNextScheduleEntry>> Entries;
#endif

	// TEMP: Instructions derived from the entries above
	UPROPERTY()
	TArray<FAnimNextScheduleInstruction> Instructions;

	// TEMP: Tasks derived from the entries above
	UPROPERTY()
	TArray<FAnimNextScheduleGraphTask> Tasks;

	// TEMP: Ports derived from the entries above
	UPROPERTY()
	TArray<FAnimNextSchedulePortTask> Ports;

	// TEMP: External tasks derived from the entries above
	UPROPERTY()
	TArray<FAnimNextScheduleExternalTask> ExternalTasks;

	// TEMP: Parameter scope entry tasks derived from the entries above
	UPROPERTY()
	TArray<FAnimNextScheduleParamScopeEntryTask> ParamScopeEntryTasks;

	// TEMP: Parameter scope exit tasks derived from the entries above
	UPROPERTY()
	TArray<FAnimNextScheduleParamScopeExitTask> ParamScopeExitTasks;

	// TEMP: Map from port name to port index in Ports array
	UPROPERTY()
	TMap<FName, int32> PortNameIndexMap;

	// TEMP: Count of total number of parameter scopes that this schedule needs to execute
	UPROPERTY()
	uint32 NumParameterScopes = 0;

	// TEMP: Count of total number of tick functions that this schedule needs to execute
	UPROPERTY()
	uint32 NumTickFunctions = 0;
};