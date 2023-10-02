// Copyright Epic Games, Inc. All Rights Reserved.

#include "ScheduleTickFunction.h"
#include "Scheduler/ScheduleContext.h"
#include "Scheduler/AnimNextSchedule.h"
#include "Scheduler/AnimNextScheduleGraphTask.h"
#include "Scheduler/AnimNextSchedulePortTask.h"
#include "Scheduler/AnimNextScheduleExternalTask.h"
#include "Scheduler/AnimNextScheduleParamScopeTask.h"
#include "Scheduler/ScheduleInstanceData.h"
#include "Scheduler/AnimNextSchedulerWorldSubsystem.h"

namespace UE::AnimNext
{

void FScheduleBeginTickFunction::ExecuteTick(float DeltaTime, ELevelTick TickType, ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
{
	// Allocate instance data if required
	if (!Entry.Context.InstanceData.IsValid())
	{
		Entry.Context.InstanceData = MakeUnique<FScheduleInstanceData>(Entry.Context, Entry.Schedule, Entry.Handle, &Entry, MoveTemp(Entry.UserScopes));
	}
}

FString FScheduleBeginTickFunction::DiagnosticMessage()
{
	return TEXT("AnimNextScheduleBeginTickFunction");
}

void FScheduleEndTickFunction::ExecuteTick(float DeltaTime, ELevelTick TickType, ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
{
}

FString FScheduleEndTickFunction::DiagnosticMessage()
{
	return TEXT("AnimNextScheduleEndTickFunction");
}

void FScheduleTickFunction::ExecuteTick(float DeltaTime, ELevelTick TickType, ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
{
	FScheduleContext::AttachToCurrentThread(ScheduleContext);

	while (!PreExecuteTasks.IsEmpty())
	{
		TOptional<TUniqueFunction<void(const UE::AnimNext::FScheduleContext&)>> Function = PreExecuteTasks.Dequeue();
		check(Function.IsSet());
		Function.GetValue()(ScheduleContext);
	}

	const UAnimNextSchedule* Schedule = ScheduleContext.Schedule;

	switch (Instruction.Opcode)
	{
	case EAnimNextScheduleScheduleOpcode::RunTask:
		{
			uint32 TaskIndex = Instruction.Operand;
			FScheduleInstanceData& InstanceData = ScheduleContext.GetInstanceData();
			FParamStack::AttachToCurrentThread(InstanceData.ParamStacks[Schedule->Tasks[TaskIndex].ParamScopeIndex]);

			Schedule->Tasks[TaskIndex].RunGraph(ScheduleContext);

			FParamStack::DetachFromCurrentThread();
			break;
		}
	case EAnimNextScheduleScheduleOpcode::BeginRunExternalTask:
		{
			if (const UObject* Object = TargetObject.Get())
			{
				FScheduleInstanceData& InstanceData = ScheduleContext.GetInstanceData();
				uint32 ExternalTaskIndex = Instruction.Operand;
				FParamStack::AddForPendingObject(Object, InstanceData.ParamStacks[Schedule->ExternalTasks[ExternalTaskIndex].ParamScopeIndex]);
			}
			break;
		}
	case EAnimNextScheduleScheduleOpcode::EndRunExternalTask:
		{
			if (const UObject* Object = TargetObject.Get())
			{
				FScheduleInstanceData& InstanceData = ScheduleContext.GetInstanceData();
				uint32 ExternalTaskIndex = Instruction.Operand;
				FParamStack::RemoveForPendingObject(Object);
			}
			break;
		}
	case EAnimNextScheduleScheduleOpcode::RunPort:
		{
			uint32 PortIndex = Instruction.Operand;
			FScheduleInstanceData& InstanceData = ScheduleContext.GetInstanceData();
			FParamStack::AttachToCurrentThread(InstanceData.ParamStacks[Schedule->Ports[PortIndex].ParamScopeIndex]);

			Schedule->Ports[PortIndex].RunPort(ScheduleContext);

			FParamStack::DetachFromCurrentThread();
			break;
		}
	case EAnimNextScheduleScheduleOpcode::RunParamScopeEntry:
		{
			uint32 ScopeEntryIndex = Instruction.Operand;
			FScheduleInstanceData& InstanceData = ScheduleContext.GetInstanceData();
			FParamStack::AttachToCurrentThread(InstanceData.ParamStacks[Schedule->ParamScopeEntryTasks[ScopeEntryIndex].ParamScopeIndex]);

			Schedule->ParamScopeEntryTasks[ScopeEntryIndex].RunParamScopeEntry(ScheduleContext);

			FParamStack::DetachFromCurrentThread();
			break;
		}
	case EAnimNextScheduleScheduleOpcode::RunParamScopeExit:
		{
			uint32 ScopeExitIndex = Instruction.Operand;
			FScheduleInstanceData& InstanceData = ScheduleContext.GetInstanceData();
			FParamStack::AttachToCurrentThread(InstanceData.ParamStacks[Schedule->ParamScopeExitTasks[ScopeExitIndex].ParamScopeIndex]);

			Schedule->ParamScopeExitTasks[ScopeExitIndex].RunParamScopeExit(ScheduleContext);

			FParamStack::DetachFromCurrentThread();
			break;
		}
	default:
		check(false);	// Unimplemented instruction
		break;
	}

	while (!PostExecuteTasks.IsEmpty())
	{
		TOptional<TUniqueFunction<void(const UE::AnimNext::FScheduleContext&)>> Function = PostExecuteTasks.Dequeue();
		check(Function.IsSet());
		Function.GetValue()(ScheduleContext);
	}

	FScheduleContext::DetachFromCurrentThread();
}

FString FScheduleTickFunction::DiagnosticMessage()
{
	return TEXT("AnimNextScheduleTickFunction");
}

}