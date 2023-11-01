// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Scheduler/AnimNextSchedule.h"
#include "Scheduler/ScheduleTickFunction.h"
#include "Scheduler/ScheduleContext.h"
#include "AnimNextSchedulerEntry.generated.h"

class UAnimNextSchedule;

namespace UE::AnimNext
{
	struct FScheduleHandle;
	struct FParamStack;
}

// Root memory owner of a parameterized schedule 
USTRUCT()
struct FAnimNextSchedulerEntry
{
	GENERATED_BODY()

	FAnimNextSchedulerEntry() = default;
	FAnimNextSchedulerEntry(const UAnimNextSchedule* InSchedule, UObject* InObject, UE::AnimNext::FScheduleHandle InHandle, EAnimNextScheduleInitMethod InInitMethod);
	~FAnimNextSchedulerEntry();

	// Setup the entry
	void Initialize(TUniqueFunction<void(const UE::AnimNext::FScheduleContext&)>&& InInitializeCallback);

	// Used for pooling
	void Invalidate();

	// Enables/disables the ticking of this entry
	void Enable(bool bInEnabled);

	// Clears the bTickEvenWhenPaused flags of the entries' tick functions
	void ClearTickFunctionPauseFlags();

	// Allocate instance data if required
	void LazyAllocateInstanceData();

	UPROPERTY(Transient)
	TObjectPtr<const UAnimNextSchedule> Schedule = nullptr;

	// Object this entry is bound to, valid only during schedule execution
	UPROPERTY(Transient)
	TObjectPtr<UObject> ResolvedObject = nullptr;

	// Object this entry is bound to
	TWeakObjectPtr<UObject> WeakObject;

	// Objects each of the schedule entries are bound to, if any
	TArray<TWeakObjectPtr<UObject>> TargetObjects;

	// Copy of the handle that represents this entry to client systems
	UE::AnimNext::FScheduleHandle Handle;

	// Root context, passed to all the schedule's tasks
	UE::AnimNext::FScheduleContext Context;

	// Root param stack for the schedule itself (and globals)
	TSharedPtr<UE::AnimNext::FParamStack> RootParamStack;
	
	// Begin/end tick functions used to wrap the schedule's tick function graph
	TUniquePtr<UE::AnimNext::FScheduleBeginTickFunction> BeginTickFunction;
	TUniquePtr<UE::AnimNext::FScheduleEndTickFunction> EndTickFunction;

	// Pre-allocated graph of tick functions
	TArray<TUniquePtr<UE::AnimNext::FScheduleTickFunction>> TickFunctions;

	// Current delta time, updated each time the schedule runs
	float DeltaTime = 0.0f;

	enum class ERunState
	{
		None,

		CreatingTasks,

		BindingTasks,

		RunningInitialUpdate,

		Running,

		Paused,
	};

	// Current running state
	ERunState RunState = ERunState::None;

	// How this entry initializes
	EAnimNextScheduleInitMethod InitMethod = EAnimNextScheduleInitMethod::InitializeAndPauseInEditor;

	// Whether this represents an editor object 
	bool bIsEditor = false;
};

template<>
struct TStructOpsTypeTraits<FAnimNextSchedulerEntry> : public TStructOpsTypeTraitsBase2<FAnimNextSchedulerEntry>
{
	enum
	{
		WithCopy = false
	};
};
