// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNextSchedulerWorldSubsystem.h"
#include "ScheduleInstanceData.h"
#include "Scheduler/AnimNextSchedule.h"
#include "Param/ParamDefinition.h"
#include "Engine/World.h"

namespace UE::AnimNext
{
static uint32 GEntrySerialNumber = 0;
}

namespace UE::AnimNext
{

FSchedulePendingAction::FSchedulePendingAction(EType InType, FScheduleHandle InHandle)
	: Handle(InHandle)
	, Type(InType)
{
}

}

bool UAnimNextSchedulerWorldSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	switch (WorldType)
	{
	case EWorldType::Game:
	case EWorldType::Editor:
	case EWorldType::PIE:
	case EWorldType::EditorPreview:
	case EWorldType::GamePreview:
		return true;
	}

	return false;
}

bool UAnimNextSchedulerWorldSubsystem::IsValidHandle(UE::AnimNext::FScheduleHandle InHandle) const
{
	return Entries.IsValidIndex(InHandle.Index) && InHandle.SerialNumber == Entries[InHandle.Index]->Handle.SerialNumber;
}

void UAnimNextSchedulerWorldSubsystem::FlushPendingActions()
{
	using namespace UE::AnimNext;

	FRWScopeLock PendingLockScope(PendingLock, SLT_Write);

	if (PendingActions.Num() > 0)
	{
		FRWScopeLock EntriesLockScope(EntriesLock, SLT_Write);

		for (FSchedulePendingAction& PendingAction : PendingActions)
		{
			switch (PendingAction.Type)
			{
			case FSchedulePendingAction::EType::ReleaseHandle:
				if (IsValidHandle(PendingAction.Handle))
				{
					if (PendingAction.Handle.Index == Entries.Num() - 1)
					{
						// Last entry, shrink array
						Entries.Pop(false);
					}
					else
					{
						// Not last entry, invalidate and add to free list
						TUniquePtr<FAnimNextSchedulerEntry>& Entry = Entries[PendingAction.Handle.Index];
						Entry->Invalidate();
						FreeEntryIndices.Add(PendingAction.Handle.Index);
					}
				}
				break;
			case FSchedulePendingAction::EType::EnableHandle:
				if (IsValidHandle(PendingAction.Handle))
				{
					TUniquePtr<FAnimNextSchedulerEntry>& Entry = Entries[PendingAction.Handle.Index];
					Entry->Enable(true);
				}
				break;
			case FSchedulePendingAction::EType::DisableHandle:
				if (IsValidHandle(PendingAction.Handle))
				{
					TUniquePtr<FAnimNextSchedulerEntry>& Entry = Entries[PendingAction.Handle.Index];
					Entry->Enable(false);
				}
				break;
			default:
				break;
			}
		}

		PendingActions.Reset();
	}
}

UE::AnimNext::FScheduleHandle UAnimNextSchedulerWorldSubsystem::AcquireHandle(UObject* InObject, UAnimNextSchedule* InSchedule, EAnimNextScheduleInitMethod InInitMethod, TUniqueFunction<void(const UE::AnimNext::FScheduleContext&)>&& InInitializeCallback)
{
	using namespace UE::AnimNext;

	FRWScopeLock EntriesLockScope(EntriesLock, SLT_Write);

	FScheduleHandle Handle;

	// Check free list first
	if (FreeEntryIndices.Num())
	{
		Handle.Index = FreeEntryIndices.Last();
		Handle.SerialNumber = ++GEntrySerialNumber;
		FreeEntryIndices.Pop(false);
		new (Entries[Handle.Index].Get()) FAnimNextSchedulerEntry(InSchedule, InObject, Handle, InInitMethod);
	}
	// Otherwise append a new entry
	else
	{
		Handle.Index = Entries.Num();
		Handle.SerialNumber = ++GEntrySerialNumber;
		Entries.Emplace(MakeUnique<FAnimNextSchedulerEntry>(InSchedule, InObject, Handle, InInitMethod));
	}

	Entries[Handle.Index]->Initialize(MoveTemp(InInitializeCallback));

	// Skip 'invalid' 0 serial number
	if (GEntrySerialNumber == 0)
	{
		++GEntrySerialNumber;
	}

	return Handle;
}

void UAnimNextSchedulerWorldSubsystem::ReleaseHandle(UE::AnimNext::FScheduleHandle InHandle)
{
	using namespace UE::AnimNext;

	if (IsValidHandle(InHandle))
	{
		FRWScopeLock EntriesLockScope(EntriesLock, SLT_Write);

		if (InHandle.Index == Entries.Num() - 1)
		{
			// Last entry, shrink array
			Entries.Pop(false);
		}
		else
		{
			// Not last entry, invalidate and add to free list
			TUniquePtr<FAnimNextSchedulerEntry>& Entry = Entries[InHandle.Index];
			Entry->Invalidate();
			FreeEntryIndices.Add(InHandle.Index);
		}

		// TODO: do not allow immediate handle releases outside of schedule runs - we can either defer or assert
	//	PendingActions.Emplace(FAnimNextSchedulePendingAction::EType::ReleaseHandle, InHandle);
	}
}

void UAnimNextSchedulerWorldSubsystem::EnableHandle(UE::AnimNext::FScheduleHandle InHandle, bool bInEnabled)
{
	using namespace UE::AnimNext;
	
	if (IsValidHandle(InHandle))
	{
		PendingActions.Emplace(bInEnabled ? FSchedulePendingAction::EType::EnableHandle : FSchedulePendingAction::EType::DisableHandle, InHandle);
	}
}

void UAnimNextSchedulerWorldSubsystem::QueueTask(UE::AnimNext::FScheduleHandle InHandle, FName InScheduleTaskName, TUniqueFunction<void(const UE::AnimNext::FScheduleContext&)>&& InTaskFunction, UE::AnimNext::FScheduler::ETaskRunLocation InLocation)
{
	using namespace UE::AnimNext;

	if (IsValidHandle(InHandle))
	{
		TUniquePtr<FAnimNextSchedulerEntry>& Entry = Entries[InHandle.Index];

		// TODO: Only supporting scope tasks for now
		const FAnimNextScheduleParamScopeEntryTask* FoundScope = Entry->Schedule->ParamScopeEntryTasks.FindByPredicate([&InScheduleTaskName](const FAnimNextScheduleParamScopeEntryTask& InTask)
		{
			return InTask.Scope == InScheduleTaskName;
		});

		if (FoundScope)
		{
			switch (InLocation)
			{
			case FScheduler::ETaskRunLocation::Before:
				Entry->TickFunctions[FoundScope->TickFunctionIndex]->PreExecuteTasks.Enqueue(MoveTemp(InTaskFunction));
				break;
			case FScheduler::ETaskRunLocation::After:
				Entry->TickFunctions[FoundScope->TickFunctionIndex]->PostExecuteTasks.Enqueue(MoveTemp(InTaskFunction));
				break;
			}
		}
		else
		{
			UE_LOG(LogAnimation, Warning, TEXT("QueueTask: Could not find scope '%s' in schedule '%s'"), *InScheduleTaskName.ToString(), *Entry->Schedule.GetName());
		}
	}
}