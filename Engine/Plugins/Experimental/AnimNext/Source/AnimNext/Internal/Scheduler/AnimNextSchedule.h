// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AnimNextScheduleGraphTask.h"
#include "Scheduler/AnimNextSchedulePortTask.h"
#include "Scheduler/AnimNextScheduleExternalTask.h"
#include "Scheduler/AnimNextScheduleParamScopeTask.h"
#include "Tasks/Task.h"
#include "Scheduler/IAnimNextScheduleTermInterface.h"
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

USTRUCT()
struct FAnimNextScheduleEntryTerm
{
	GENERATED_BODY()

	FAnimNextScheduleEntryTerm() = default;
	
	FAnimNextScheduleEntryTerm(FName InName, const FAnimNextParamType& InType, EScheduleTermDirection InDirection)
		: Name(InName)
		, Type(InType)
		, Direction(InDirection)
	{}

	UPROPERTY(EditAnywhere, Category = "Term")
	FName Name;

	UPROPERTY(EditAnywhere, Category = "Term")
	FAnimNextParamType Type;

	UPROPERTY(EditAnywhere, Category = "Term")
	EScheduleTermDirection Direction = EScheduleTermDirection::Input;
};

UCLASS(EditInlineNew, Abstract)
class UAnimNextScheduleEntry : public UObject
{
	GENERATED_BODY()

private:
	friend class UAnimNextSchedule;
};

UCLASS(DisplayName="Graph")
class UAnimNextScheduleEntry_AnimNextGraph : public UAnimNextScheduleEntry
{
	GENERATED_BODY()

private:
	friend class UAnimNextSchedule;

	// The graph to run by default
	UPROPERTY(EditAnywhere, Category = "Graph")
	TObjectPtr<UAnimNextGraph> Graph = nullptr;

	// Parameter to get the graph from dynamically
	UPROPERTY(EditAnywhere, Category = "Graph", meta = (CustomWidget = "ParamName", AllowedParamType = "TObjectPtr<UAnimNextGraph>"))
	FName DynamicGraph;

	// An optional entry point to use when running the supplied graph
	UPROPERTY(EditAnywhere, Category = "Graph", meta = (CustomWidget = "ParamName", AllowedParamType = "FName"))
	FName EntryPoint;

	// The intermediate terms used by the graph
	UPROPERTY(EditAnywhere, Category = "Graph")
	TArray<FAnimNextScheduleEntryTerm> Terms;
};

UCLASS(DisplayName="Port")
class UAnimNextScheduleEntry_Port : public UAnimNextScheduleEntry
{
	GENERATED_BODY()

private:
	friend class UAnimNextSchedule;

	// The type of the port to use
	UPROPERTY(EditAnywhere, Category = "Port", meta = (ShowDisplayNames))
	TSubclassOf<UAnimNextSchedulePort> Port;

	// The intermediate terms used by this port
	UPROPERTY(EditAnywhere, Category = "Port")
	TArray<FAnimNextScheduleEntryTerm> Terms;
};

UCLASS(DisplayName="External")
class UAnimNextScheduleEntry_ExternalTask : public UAnimNextScheduleEntry
{
	GENERATED_BODY()

private:
	friend class UAnimNextSchedule;

	// The tick function that this external task should wrap
	UPROPERTY(EditAnywhere, Category = "External Task", meta = (CustomWidget = "ParamName", AllowedParamType = "FTickFunction"))
	FName TickFunction;

	// The object that the tick function is present on
	UPROPERTY(EditAnywhere, Category = "External Task", meta = (CustomWidget = "ParamName", AllowedParamType = "TObjectPtr<UObject>"))
	FName Object;
};

UCLASS(DisplayName="Scope")
class UAnimNextScheduleEntry_ParamScope : public UAnimNextScheduleEntry
{
	GENERATED_BODY()

private:
	friend class UAnimNextSchedule;

	// The scope to use
	UPROPERTY(EditAnywhere, Category = "Parameters", meta = (CustomWidget = "ParamName", AllowedParamType = "FAnimNextScope"))
	FName Scope;

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
	EAnimNextScheduleScheduleOpcode Opcode = EAnimNextScheduleScheduleOpcode::None;

	UPROPERTY()
	int32 Operand = INDEX_NONE;
};

UENUM()
enum class EAnimNextScheduleInitMethod : uint8
{
	// Do not perform any initial update, set up data structures only
	None,

	// Set up data structures, perform an initial update and then pause
	InitializeAndPause,

	// Set up data structures, perform an initial update and then pause in editor only, otherwise act like InitializeAndRun
	InitializeAndPauseInEditor,

	// Set up data structures then continue updating
	InitializeAndRun
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
	virtual void PostEditUndo() override;

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
	UPROPERTY(NonTransactional)
	TArray<FAnimNextScheduleInstruction> Instructions;

	// TEMP: Tasks derived from the entries above
	UPROPERTY(NonTransactional)
	TArray<FAnimNextScheduleGraphTask> Tasks;

	// TEMP: Ports derived from the entries above
	UPROPERTY(NonTransactional)
	TArray<FAnimNextSchedulePortTask> Ports;

	// TEMP: External tasks derived from the entries above
	UPROPERTY(NonTransactional)
	TArray<FAnimNextScheduleExternalTask> ExternalTasks;

	// TEMP: Parameter scope entry tasks derived from the entries above
	UPROPERTY(NonTransactional)
	TArray<FAnimNextScheduleParamScopeEntryTask> ParamScopeEntryTasks;

	// TEMP: Parameter scope exit tasks derived from the entries above
	UPROPERTY(NonTransactional)
	TArray<FAnimNextScheduleParamScopeExitTask> ParamScopeExitTasks;

	// TEMP: Data for intermediates, defined as a property bag
	UPROPERTY(NonTransactional)
	FInstancedPropertyBag IntermediatesData;

	// TEMP: Count of total number of parameter scopes that this schedule needs to execute
	UPROPERTY(NonTransactional)
	uint32 NumParameterScopes = 0;

	// TEMP: Count of total number of tick functions that this schedule needs to execute
	UPROPERTY(NonTransactional)
	uint32 NumTickFunctions = 0;
};