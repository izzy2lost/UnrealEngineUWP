// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
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
	FAnimNextSchedulerEntry(const UAnimNextSchedule* InSchedule, UObject* InObject, UE::AnimNext::FScheduleHandle InHandle, const TMap<FName, FAnimNextParameterCollection>& InUserScopes);
	~FAnimNextSchedulerEntry();

	// Used for pooling
	void Invalidate();

	// Enables/disables the ticking of this entry
	void Enable(bool bInEnabled);
	
	UPROPERTY()
	TObjectPtr<const UAnimNextSchedule> Schedule = nullptr;

	// User scopes are copied into this entry on construction, but moved out later into instance data
	// So will be invalid here after first run
	UPROPERTY()
	TMap<FName, FAnimNextParameterCollection> UserScopes;

	// Object this entry is bound to
	TWeakObjectPtr<UObject> Object;

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
};

template<>
struct TStructOpsTypeTraits<FAnimNextSchedulerEntry> : public TStructOpsTypeTraitsBase2<FAnimNextSchedulerEntry>
{
	enum
	{
		WithCopy = false
	};
};
