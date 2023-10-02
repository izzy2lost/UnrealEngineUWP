// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNextSchedulerEntry.h"
#include "ScheduleInstanceData.h"
#include "Scheduler/AnimNextSchedule.h"
#include "Engine/World.h"

FAnimNextSchedulerEntry::FAnimNextSchedulerEntry(const UAnimNextSchedule* InSchedule, UObject* InObject, UE::AnimNext::FScheduleHandle InHandle, const TMap<FName, FAnimNextParameterCollection>& InUserScopes)
	: Schedule(InSchedule)
	, UserScopes(InUserScopes)
	, Object(InObject)
	, Handle(InHandle)
	, Context(InSchedule, this)
{
	using namespace UE::AnimNext;

	check(InObject)
	check(InSchedule);
	check(InHandle.IsValid());

	RootParamStack = MakeShared<FParamStack>();

	FParamStack::AttachToCurrentThread(RootParamStack);
	FScheduleContext::AttachToCurrentThread(Context);

	// Setup tick function graph
	if (InSchedule->Instructions.Num() > 0)
	{
		BeginTickFunction = MakeUnique<FScheduleBeginTickFunction>(*this);
		EndTickFunction = MakeUnique<FScheduleEndTickFunction>(*this);

		TArray<TUniquePtr<FScheduleTickFunction>*> Prerequisites;
		int32 InstructionIndex = 0;
		while (InstructionIndex < InSchedule->Instructions.Num())
		{
			auto AddTickFunctionAndPrerequisites = [this, &Prerequisites, InObject](TUniquePtr<FScheduleTickFunction>&& InTickFunction)
			{
				// Add a prereq on the previous tick function we added, if any (we only support linear chains right now so this is OK)
				if (TickFunctions.Num() == 0)
				{
					BeginTickFunction->Subsequent = FTickPrerequisite(InObject, *InTickFunction.Get());
					InTickFunction->AddPrerequisite(InObject, *BeginTickFunction.Get());
				}

				for (TUniquePtr<FScheduleTickFunction>* Prerequisite : Prerequisites)
				{
					Prerequisite->Get()->Subsequents.Emplace(InObject, *InTickFunction.Get());
					InTickFunction->AddPrerequisite(InObject, *Prerequisite->Get());
				}
				Prerequisites.Reset();
				TickFunctions.Add(MoveTemp(InTickFunction));
			};

			const FAnimNextScheduleInstruction& Instruction = InSchedule->Instructions[InstructionIndex++];
			switch (Instruction.Opcode)
			{
			case EAnimNextScheduleScheduleOpcode::BeginRunExternalTask:
				{
					AddTickFunctionAndPrerequisites(MakeUnique<FScheduleTickFunction>(Context, Instruction));

					const FName TickFunctionName = InSchedule->ExternalTasks[Instruction.Operand].Name;
					const FName ObjectName = InSchedule->ExternalTasks[Instruction.Operand].ObjectName;
					if(TickFunctionName != NAME_None && ObjectName != NAME_None)
					{
						FTickFunction* FoundFunction = FParamStack::Get().GetMutableParamPtr<FTickFunction>(TickFunctionName);
						const UObject* FoundObject = FParamStack::Get().GetParamPtr<UObject>(ObjectName);
						if(FoundFunction && FoundObject)
						{
							FoundFunction->AddPrerequisite(InObject, *TickFunctions.Last().Get());
							TickFunctions.Last()->TargetObject = const_cast<UObject*>(FoundObject);
							TickFunctions.Last()->Subsequents.Emplace(const_cast<UObject*>(FoundObject), *FoundFunction);
						}
					}
					break;
				}
			case EAnimNextScheduleScheduleOpcode::EndRunExternalTask:
				{
					AddTickFunctionAndPrerequisites(MakeUnique<FScheduleTickFunction>(Context, Instruction));

					const FName TickFunctionName = InSchedule->ExternalTasks[Instruction.Operand].Name;
					const FName ObjectName = InSchedule->ExternalTasks[Instruction.Operand].ObjectName;
					if(TickFunctionName != NAME_None && ObjectName != NAME_None)
					{
						FTickFunction* FoundFunction = FParamStack::Get().GetMutableParamPtr<FTickFunction>(TickFunctionName);
						const UObject* FoundObject = FParamStack::Get().GetParamPtr<UObject>(ObjectName);
						if(FoundFunction && FoundObject)
						{
							TickFunctions.Last()->TargetObject = const_cast<UObject*>(FoundObject);
							TickFunctions.Last()->AddPrerequisite(const_cast<UObject*>(FoundObject), *FoundFunction);
						}
					}
					break;
				}
			case EAnimNextScheduleScheduleOpcode::RunTask:
			case EAnimNextScheduleScheduleOpcode::RunPort:
			case EAnimNextScheduleScheduleOpcode::RunParamScopeEntry:
			case EAnimNextScheduleScheduleOpcode::RunParamScopeExit:
				AddTickFunctionAndPrerequisites(MakeUnique<FScheduleTickFunction>(Context, Instruction));
				break;
			case EAnimNextScheduleScheduleOpcode::PrerequisiteTask:
			case EAnimNextScheduleScheduleOpcode::PrerequisiteBeginExternalTask:
			case EAnimNextScheduleScheduleOpcode::PrerequisiteEndExternalTask:
			case EAnimNextScheduleScheduleOpcode::PrerequisiteScopeEntry:
			case EAnimNextScheduleScheduleOpcode::PrerequisiteScopeExit:
				check(TickFunctions[Instruction.Operand].IsValid());
				Prerequisites.Add(&TickFunctions[Instruction.Operand]);
				break;
			case EAnimNextScheduleScheduleOpcode::Exit:
				check(Prerequisites.Num() == 0);
				EndTickFunction->AddPrerequisite(InObject, *TickFunctions.Last().Get());
				TickFunctions.Last()->Subsequents.Emplace(InObject, *EndTickFunction.Get());
				break;
			default:
				check(false);
				break;
			}
		}

		// Register our tick functions
		ULevel* Level = InObject->GetWorld()->PersistentLevel;
		BeginTickFunction->RegisterTickFunction(Level);
		EndTickFunction->RegisterTickFunction(Level);
		for (TUniquePtr<FScheduleTickFunction>& TickFunction : TickFunctions)
		{
			TickFunction->RegisterTickFunction(Level);
		}
	}

	FScheduleContext::DetachFromCurrentThread();
	FParamStack::DetachFromCurrentThread();
}

FAnimNextSchedulerEntry::~FAnimNextSchedulerEntry()
{
	Invalidate();
}

void FAnimNextSchedulerEntry::Invalidate()
{
	using namespace UE::AnimNext;

	if(BeginTickFunction)
	{
		BeginTickFunction->Subsequent.PrerequisiteTickFunction->RemovePrerequisite(Object.Get(), *BeginTickFunction.Get());
	}
	
	for (TUniquePtr<FScheduleTickFunction>& TickFunction : TickFunctions)
	{
		for (FTickPrerequisite& Subsequent : TickFunction->Subsequents)
		{
			Subsequent.PrerequisiteTickFunction->RemovePrerequisite(Object.Get(), *TickFunction.Get());
		}
		TickFunction->UnRegisterTickFunction();
	}

	if (BeginTickFunction)
	{
		BeginTickFunction->UnRegisterTickFunction();
	}
	if(EndTickFunction)
	{
		EndTickFunction->UnRegisterTickFunction();
	}

	BeginTickFunction.Reset();
	EndTickFunction.Reset();
	TickFunctions.Reset();

	Schedule = nullptr;
	Object = nullptr;
	UserScopes.Empty();
	Handle.Invalidate();
	Context.InstanceData.Reset();
}

void FAnimNextSchedulerEntry::Enable(bool bInEnabled)
{
	using namespace UE::AnimNext;
	
	BeginTickFunction->SetTickFunctionEnable(bInEnabled);
	for (TUniquePtr<FScheduleTickFunction>& TickFunction : TickFunctions)
	{
		TickFunction->SetTickFunctionEnable(bInEnabled);
	}
	EndTickFunction->SetTickFunctionEnable(bInEnabled);
}