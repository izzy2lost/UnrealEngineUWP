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
	Entry.ResolvedObject = Entry.WeakObject.Get();
	Entry.DeltaTime = DeltaTime;
}

FString FScheduleBeginTickFunction::DiagnosticMessage()
{
	return TEXT("AnimNextScheduleBeginTickFunction");
}

void FScheduleEndTickFunction::ExecuteTick(float DeltaTime, ELevelTick TickType, ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
{
	if(Entry.RunState == FAnimNextSchedulerEntry::ERunState::RunningInitialUpdate)
	{
		if( Entry.InitMethod == EAnimNextScheduleInitMethod::InitializeAndPause
#if WITH_EDITOR
			|| (Entry.InitMethod == EAnimNextScheduleInitMethod::InitializeAndPauseInEditor && Entry.bIsEditor)
#endif
			)
		{
			// Queue task to disable our tick functions now we have performed our initial update
			FFunctionGraphTask::CreateAndDispatchWhenReady(
			[this]()
			{
				check(IsInGameThread());

				Entry.Enable(false);
				Entry.RunState = FAnimNextSchedulerEntry::ERunState::Paused;
			},
			TStatId(),
			nullptr,
			ENamedThreads::GameThread);
		}
	}
	else
	{
		FFunctionGraphTask::CreateAndDispatchWhenReady(
		[this]()
		{
			check(IsInGameThread());

			Entry.RunState = FAnimNextSchedulerEntry::ERunState::Running;
		},
		TStatId(),
		nullptr,
		ENamedThreads::GameThread);
	}

	Entry.ResolvedObject = nullptr;
}

FString FScheduleEndTickFunction::DiagnosticMessage()
{
	return TEXT("AnimNextScheduleEndTickFunction");
}

void FScheduleTickFunction::ExecuteTick(float DeltaTime, ELevelTick TickType, ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
{
	FScheduleContext::AttachToCurrentThread(ScheduleContext);

	RunSchedule(Instructions, TargetObjects,
		[this]()
		{
			while (!PreExecuteTasks.IsEmpty())
			{
				TOptional<TUniqueFunction<void(const UE::AnimNext::FScheduleContext&)>> Function = PreExecuteTasks.Dequeue();
				check(Function.IsSet());
				Function.GetValue()(ScheduleContext);
			}
		}, 
		[this]()
		{
			while (!PostExecuteTasks.IsEmpty())
			{
				TOptional<TUniqueFunction<void(const UE::AnimNext::FScheduleContext&)>> Function = PostExecuteTasks.Dequeue();
				check(Function.IsSet());
				Function.GetValue()(ScheduleContext);
			}
		});

	FScheduleContext::DetachFromCurrentThread();
}

void FScheduleTickFunction::RunSchedule(TConstArrayView<FAnimNextScheduleInstruction> InInstructions)
{
	RunSchedule(InInstructions, {}, [](){}, [](){});
}

void FScheduleTickFunction::RunSchedule(TConstArrayView<FAnimNextScheduleInstruction> InInstructions, TConstArrayView<TWeakObjectPtr<UObject>> InTargetObjects, TFunctionRef<void(void)> InPreExecuteScope, TFunctionRef<void(void)> InPostExecuteScope)
{
	const FScheduleContext& ScheduleContext = FScheduleContext::Get();

	const UAnimNextSchedule* Schedule = ScheduleContext.Schedule;

	int32 InstructionIndex = 0;
	while(InstructionIndex < InInstructions.Num())
	{
		const FAnimNextScheduleInstruction& Instruction = InInstructions[InstructionIndex];
		switch (Instruction.Opcode)
		{
		case EAnimNextScheduleScheduleOpcode::RunGraphTask:
			{
				uint32 GraphTaskIndex = Instruction.Operand;
				FScheduleInstanceData& InstanceData = ScheduleContext.GetInstanceData();
				FParamStack::AttachToCurrentThread(InstanceData.GetParamStack(Schedule->GraphTasks[GraphTaskIndex].ParamScopeIndex), FParamStack::ECoalesce::Coalesce);

				Schedule->GraphTasks[GraphTaskIndex].RunGraph(ScheduleContext);

				FParamStack::DetachFromCurrentThread(FParamStack::EDecoalesce::Decoalesce);
				break;
			}
		case EAnimNextScheduleScheduleOpcode::BeginRunExternalTask:
			{
				if(InTargetObjects.Num() > 0)
				{
					if (const UObject* Object = InTargetObjects[InstructionIndex].Get())
					{
						FScheduleInstanceData& InstanceData = ScheduleContext.GetInstanceData();
						uint32 ExternalTaskIndex = Instruction.Operand;
						FParamStack::AddForPendingObject(Object, InstanceData.GetParamStack(Schedule->ExternalTasks[ExternalTaskIndex].ParamScopeIndex));
					}
				}
				break;
			}
		case EAnimNextScheduleScheduleOpcode::EndRunExternalTask:
			{
				if(InTargetObjects.Num() > 0)
				{
					if (const UObject* Object = InTargetObjects[InstructionIndex].Get())
					{
						FParamStack::RemoveForPendingObject(Object);
					}
				}
				break;
			}
		case EAnimNextScheduleScheduleOpcode::RunPort:
			{
				uint32 PortIndex = Instruction.Operand;
				FScheduleInstanceData& InstanceData = ScheduleContext.GetInstanceData();
				FParamStack::AttachToCurrentThread(InstanceData.GetParamStack(Schedule->Ports[PortIndex].ParamScopeIndex));

				Schedule->Ports[PortIndex].RunPort(ScheduleContext);

				FParamStack::DetachFromCurrentThread();
				break;
			}
		case EAnimNextScheduleScheduleOpcode::RunParamScopeEntry:
			{
				InPreExecuteScope();

				uint32 ScopeEntryIndex = Instruction.Operand;
				FScheduleInstanceData& InstanceData = ScheduleContext.GetInstanceData();
				FParamStack::AttachToCurrentThread(InstanceData.GetParamStack(Schedule->ParamScopeEntryTasks[ScopeEntryIndex].ParamScopeIndex), FParamStack::ECoalesce::Coalesce);

				Schedule->ParamScopeEntryTasks[ScopeEntryIndex].RunParamScopeEntry(ScheduleContext);

				FParamStack::DetachFromCurrentThread();

				InPostExecuteScope();

				break;
			}
		case EAnimNextScheduleScheduleOpcode::RunParamScopeExit:
			{
				uint32 ScopeExitIndex = Instruction.Operand;
				FScheduleInstanceData& InstanceData = ScheduleContext.GetInstanceData();
				FParamStack::AttachToCurrentThread(InstanceData.GetParamStack(Schedule->ParamScopeExitTasks[ScopeExitIndex].ParamScopeIndex));

				Schedule->ParamScopeExitTasks[ScopeExitIndex].RunParamScopeExit(ScheduleContext);

				FParamStack::DetachFromCurrentThread(FParamStack::EDecoalesce::Decoalesce);
				break;
			}
		case EAnimNextScheduleScheduleOpcode::RunExternalParamTask:
			{
				uint32 ExternalParamIndex = Instruction.Operand;
				FScheduleInstanceData& InstanceData = ScheduleContext.GetInstanceData();
				FParamStack::AttachToCurrentThread(InstanceData.RootParamStack);

				Schedule->ExternalParamTasks[ExternalParamIndex].UpdateExternalParams(ScheduleContext);

				FParamStack::DetachFromCurrentThread();
				break;
			}
		default:
			break;
		}

		InstructionIndex++;
	}
}

FString FScheduleTickFunction::DiagnosticMessage()
{
	return TEXT("AnimNextScheduleTickFunction");
}

}