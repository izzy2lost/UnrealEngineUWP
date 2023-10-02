// Copyright Epic Games, Inc. All Rights Reserved.

#include "Scheduler/AnimNextSchedule.h"
#include "Tasks/Task.h"
#include "Async/TaskGraphInterfaces.h"
#include "UObject/ObjectSaveContext.h"
#include "EngineLogs.h"

void UAnimNextSchedule::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITOR
	CompileSchedule();
#endif
}

#if WITH_EDITOR

void UAnimNextSchedule::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	CompileSchedule();
}

void UAnimNextSchedule::CompileSchedule()
{
	Instructions.Empty();
	Tasks.Empty();
	Ports.Empty();
	ExternalTasks.Empty();
	ParamScopeEntryTasks.Empty();
	ParamScopeExitTasks.Empty();
	PortNameIndexMap.Empty();
	NumParameterScopes = 0;
	NumTickFunctions = 0;

	EAnimNextScheduleScheduleOpcode LastOpCode = EAnimNextScheduleScheduleOpcode::None;

	auto Emit = [this, &LastOpCode](EAnimNextScheduleScheduleOpcode InOpCode, int32 InOperand = 0)
	{
		FAnimNextScheduleInstruction Instruction;
		Instruction.Opcode = InOpCode;
		Instruction.Operand = InOperand;
		Instructions.Add(Instruction);

		LastOpCode = InOpCode;
	};

	auto EmitPrerequisite = [this, &Emit, &LastOpCode]()
	{
		switch (LastOpCode)
		{
		case EAnimNextScheduleScheduleOpcode::RunTask:
			Emit(EAnimNextScheduleScheduleOpcode::PrerequisiteTask, NumTickFunctions - 1);
			break;
		case EAnimNextScheduleScheduleOpcode::BeginRunExternalTask:
			Emit(EAnimNextScheduleScheduleOpcode::PrerequisiteBeginExternalTask, NumTickFunctions - 1);
			break;
		case EAnimNextScheduleScheduleOpcode::EndRunExternalTask:
			Emit(EAnimNextScheduleScheduleOpcode::PrerequisiteEndExternalTask, NumTickFunctions - 1);
			break;
		case EAnimNextScheduleScheduleOpcode::RunParamScopeEntry:
			Emit(EAnimNextScheduleScheduleOpcode::PrerequisiteScopeEntry, NumTickFunctions - 1);
			break;
		case EAnimNextScheduleScheduleOpcode::RunParamScopeExit:
			Emit(EAnimNextScheduleScheduleOpcode::PrerequisiteScopeExit, NumTickFunctions - 1);
			break;
		default:
			break;
		}
	};

	// MAX_uint32 means 'global scope' in this context
	uint32 ParentScopeIndex = MAX_uint32;

	TFunction<void(const TArray<TObjectPtr<UAnimNextScheduleEntry>>&)> EmitEntries;

	// Iterate over all entries, recursing into scopes
	EmitEntries = [this, &EmitEntries, &Emit, &EmitPrerequisite, &ParentScopeIndex](const TArray<TObjectPtr<UAnimNextScheduleEntry>>& InEntries)
	{
		for (int32 EntryIndex = 0; EntryIndex < InEntries.Num(); ++EntryIndex)
		{
			UAnimNextScheduleEntry* Entry = InEntries[EntryIndex];

			if (UAnimNextScheduleEntry_Port* PortEntry = Cast<UAnimNextScheduleEntry_Port>(Entry))
			{
				EmitPrerequisite();

				FAnimNextSchedulePortTask PortTask;
				PortTask.TaskIndex = Ports.Num();
				PortTask.ParamScopeIndex = ParentScopeIndex;
				PortTask.Name = PortEntry->Name;
				int32 PortIndex = Ports.Add(PortTask);
				PortNameIndexMap.Add(PortEntry->Name, PortIndex);

				NumTickFunctions++;

				Emit(EAnimNextScheduleScheduleOpcode::RunPort, PortIndex);
			}
			else if (UAnimNextScheduleEntry_AnimNextGraph* GraphEntry = Cast<UAnimNextScheduleEntry_AnimNextGraph>(Entry))
			{
				EmitPrerequisite();

				FAnimNextScheduleGraphTask Task;
				Task.TaskIndex = Tasks.Num();
				Task.ParamScopeIndex = NumParameterScopes++;
				Task.ParamParentScopeIndex = ParentScopeIndex;
				Task.Name = GraphEntry->Name;
				Task.EntryPoint = GraphEntry->EntryPoint;
				Task.Graph = GraphEntry->Graph;
				Task.ParameterBlocks = GraphEntry->ParameterBlocks;
				int32 TaskIndex = Tasks.Add(Task);

				NumTickFunctions++;

				Emit(EAnimNextScheduleScheduleOpcode::RunTask, TaskIndex);
			}
			else if (UAnimNextScheduleEntry_ExternalTask* ExternalTaskEntry = Cast<UAnimNextScheduleEntry_ExternalTask>(Entry))
			{
				EmitPrerequisite();

				FAnimNextScheduleExternalTask ExternalTask;
				ExternalTask.TaskIndex = ExternalTasks.Num();
				ExternalTask.ParamScopeIndex = NumParameterScopes++;
				ExternalTask.ParamParentScopeIndex = ParentScopeIndex;
				ExternalTask.Name = ExternalTaskEntry->Name;
				ExternalTask.ObjectName = ExternalTaskEntry->ObjectName;
				int32 ExternalTaskIndex = ExternalTasks.Add(ExternalTask);

				// Emit the external task
				Emit(EAnimNextScheduleScheduleOpcode::BeginRunExternalTask, ExternalTaskIndex);
				NumTickFunctions++;

				EmitPrerequisite();

				Emit(EAnimNextScheduleScheduleOpcode::EndRunExternalTask, ExternalTaskIndex);
				NumTickFunctions++;
			}
			else if (UAnimNextScheduleEntry_ParamScope* ParamScopeTaskEntry = Cast<UAnimNextScheduleEntry_ParamScope>(Entry))
			{
				EmitPrerequisite();

				FAnimNextScheduleParamScopeEntryTask ParamScopeEntryTask;
				ParamScopeEntryTask.TaskIndex = ParamScopeEntryTasks.Num();
				const uint32 ParamScopeIndex = NumParameterScopes++;
				ParamScopeEntryTask.ParamScopeIndex = ParamScopeIndex;
				ParamScopeEntryTask.ParamParentScopeIndex = ParentScopeIndex;
				ParamScopeEntryTask.TickFunctionIndex = NumTickFunctions;
				ParamScopeEntryTask.Name = ParamScopeTaskEntry->Name;
				ParamScopeEntryTask.ParameterBlocks = ParamScopeTaskEntry->ParameterBlocks;
				int32 ParamScopeTaskEntryIndex = ParamScopeEntryTasks.Add(ParamScopeEntryTask);

				Emit(EAnimNextScheduleScheduleOpcode::RunParamScopeEntry, ParamScopeTaskEntryIndex);
				NumTickFunctions++;

				// Enter new scope
				uint32 PreviousParentScope = ParentScopeIndex;
				ParentScopeIndex = ParamScopeIndex;

				// Emit the subentries
				EmitEntries(ParamScopeTaskEntry->SubEntries);

				// Exit scope
				ParentScopeIndex = PreviousParentScope;

				EmitPrerequisite();

				FAnimNextScheduleParamScopeExitTask ParamScopeExitTask;
				ParamScopeExitTask.TaskIndex = ParamScopeExitTasks.Num();
				ParamScopeExitTask.ParamScopeIndex = ParamScopeIndex;
				ParamScopeExitTask.Name = ParamScopeTaskEntry->Name;
				int32 ParamScopeExitTaskIndex = ParamScopeExitTasks.Add(ParamScopeExitTask);

				Emit(EAnimNextScheduleScheduleOpcode::RunParamScopeExit, ParamScopeExitTaskIndex);
				NumTickFunctions++;
			}
		}
	};

	EmitEntries(Entries);

	Emit(EAnimNextScheduleScheduleOpcode::Exit);
}

#endif // #if WITH_EDITOR