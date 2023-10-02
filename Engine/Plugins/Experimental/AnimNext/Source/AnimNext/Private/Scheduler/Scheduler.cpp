// Copyright Epic Games, Inc. All Rights Reserved.

#include "Scheduler/Scheduler.h"
#include "Scheduler/SchedulePortAdapterContext.h"
#include "Scheduler/SchedulePortDefinition.h"
#include "Scheduler/ScheduleContext.h"
#include "Scheduler/AnimNextSchedule.h"
#include "Scheduler/AnimNextSchedulerWorldSubsystem.h"
#include "Tasks/Task.h"
#include "UObject/GCObject.h"
#include "LODPose.h"
#include "Param/Params.h"
#include "Engine/World.h"
#include "Components/SkeletalMeshComponent.h"
#include "UObject/ObjectKey.h"

namespace UE::AnimNext
{

struct FSchedulerImpl
{
	void RegisterBuiltInPortDefinitions()
	{
/*		RegisteredPortDefinitions.Add(
			Ports::SkeletalMeshComponentPose.Name,
			FSchedulePortDefinition(
				Ports::SkeletalMeshComponentPose.Name,
				FParamTypeHandle::GetHandle<TArray<FAnimNextLODPose>>(),
				FParamTypeHandle::GetHandle<TArray<FTransform>>(),
				[](const FSchedulePortAdapterContext& InContext)
				{
					check(InContext.InputType == FParamTypeHandle::GetHandle<FAnimNextLODPose>());
					check(InContext.OutputType == FParamTypeHandle::GetHandle<TArray<FTransform>>());

					USkeletalMeshComponent* OutputObject = CastChecked<USkeletalMeshComponent>(InContext.OutputObject);
				
					// TODO: Perform pose transpose
				}));*/
	}

	FDelegateHandle OnWorldPreActorTickHandle;

	TMap<FName, FSchedulePortDefinition> RegisteredPortDefinitions;

	uint32 SerialNumber = 0;
};

static FSchedulerImpl Impl;

void FScheduler::Init()
{
	Impl.RegisterBuiltInPortDefinitions();

	// Kick off root task at the start of each world tick
	Impl.OnWorldPreActorTickHandle = FWorldDelegates::OnWorldPreActorTick.AddLambda([](UWorld* InWorld, ELevelTick InTickType, float InDeltaSeconds)
	{
		if (InTickType == LEVELTICK_All || InTickType == LEVELTICK_ViewportsOnly)
		{
			if(UAnimNextSchedulerWorldSubsystem* Subsystem = InWorld->GetSubsystem<UAnimNextSchedulerWorldSubsystem>())
			{
				// Flush actions here as they require game thread callbacks (e.g. to reconfigure tick functions)
				Subsystem->FlushPendingActions();
				Subsystem->DeltaTime = InDeltaSeconds;
			}
		}
	});
}

void FScheduler::Destroy()
{
	FWorldDelegates::OnWorldPreActorTick.Remove(Impl.OnWorldPreActorTickHandle);
}

FScheduleHandle FScheduler::AcquireHandle(UObject* InObject, UAnimNextSchedule* InSchedule, const TMap<FName, FAnimNextParameterCollection>& InUserScopes)
{
	FScheduleHandle Handle;

	// Check parameters
	if (InSchedule == nullptr)
	{
		UE_LOG(LogAnimation, Warning, TEXT("FScheduler::AcquireHandle: Invalid schedule"));
		return FScheduleHandle();
	}

	if (InObject == nullptr)
	{
		UE_LOG(LogAnimation, Warning, TEXT("FScheduler::AcquireHandle: Invalid object"));
		return FScheduleHandle();
	}

	UWorld* World = InObject->GetWorld();
	UAnimNextSchedulerWorldSubsystem* Subsystem = World->GetSubsystem<UAnimNextSchedulerWorldSubsystem>();
	return Subsystem->AcquireHandle(InObject, InSchedule, InUserScopes);
}

void FScheduler::ReleaseHandle(UObject* InObject, FScheduleHandle& InHandle)
{
	if(InHandle.IsValid())
	{
		UWorld* World = InObject->GetWorld();
		UAnimNextSchedulerWorldSubsystem* Subsystem = World->GetSubsystem<UAnimNextSchedulerWorldSubsystem>();
		Subsystem->ReleaseHandle(InHandle);
		InHandle.Invalidate();
	}
}

void FScheduler::EnableHandle(UObject* InObject, FScheduleHandle InHandle, bool bInEnabled)
{
	if (InHandle.IsValid())
	{
		UWorld* World = InObject->GetWorld();
		UAnimNextSchedulerWorldSubsystem* Subsystem = World->GetSubsystem<UAnimNextSchedulerWorldSubsystem>();
		Subsystem->EnableHandle(InHandle, bInEnabled);
	}
}

void FScheduler::QueueTask(UObject* InObject, FScheduleHandle InHandle, FName InScheduleTaskName, TUniqueFunction<void(const FScheduleContext&)>&& InTaskFunction, ETaskRunLocation InLocation)
{
	if (InHandle.IsValid())
	{
		UWorld* World = InObject->GetWorld();
		UAnimNextSchedulerWorldSubsystem* Subsystem = World->GetSubsystem<UAnimNextSchedulerWorldSubsystem>();
		Subsystem->QueueTask(InHandle, InScheduleTaskName, MoveTemp(InTaskFunction), InLocation);
	}
}

void FScheduler::RegisterPortDefinition(FSchedulePortDefinition&& InPortDefinition)
{
	Impl.RegisteredPortDefinitions.Add(InPortDefinition.Name, MoveTemp(InPortDefinition));
}

void FScheduler::UnregisterPortDefinition(FName InDefinitionName)
{
	Impl.RegisteredPortDefinitions.Remove(InDefinitionName);
}

const FSchedulePortDefinition* FScheduler::FindPortDefinition(FName InDefinitionName)
{
	return Impl.RegisteredPortDefinitions.Find(InDefinitionName);
}

}