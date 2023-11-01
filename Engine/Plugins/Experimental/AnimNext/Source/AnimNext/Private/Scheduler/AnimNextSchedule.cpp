// Copyright Epic Games, Inc. All Rights Reserved.

#include "Scheduler/AnimNextSchedule.h"
#include "Tasks/Task.h"
#include "Async/TaskGraphInterfaces.h"
#include "EngineLogs.h"
#include "Scheduler/AnimNextSchedulePort.h"
#include "Graph/AnimNextGraph.h"

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

void UAnimNextSchedule::PostEditUndo()
{
	Super::PostEditUndo();

	CompileSchedule();
}

void UAnimNextSchedule::CompileSchedule()
{
	using namespace UE::AnimNext;

	Instructions.Empty();
	Tasks.Empty();
	Ports.Empty();
	ExternalTasks.Empty();
	ParamScopeEntryTasks.Empty();
	ParamScopeExitTasks.Empty();
	IntermediatesData.Reset();
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

	TArray<FAnimNextScheduleEntryTerm> IntermediateTerms;
	TMap<FName, uint32> IntermediateMap;

	TFunction<void(const TArray<TObjectPtr<UAnimNextScheduleEntry>>&)> EmitEntries;

	// Iterate over all entries, recursing into scopes
	EmitEntries = [this, &EmitEntries, &Emit, &EmitPrerequisite, &ParentScopeIndex, &IntermediateTerms, &IntermediateMap](const TArray<TObjectPtr<UAnimNextScheduleEntry>>& InEntries)
	{
		for (int32 EntryIndex = 0; EntryIndex < InEntries.Num(); ++EntryIndex)
		{
			UAnimNextScheduleEntry* Entry = InEntries[EntryIndex];

			auto CheckTermDirectionCompatibility = [](FName InName, EScheduleTermDirection InExistingDirection, EScheduleTermDirection InNewDirection)
			{
				switch(InExistingDirection)
				{
				case EScheduleTermDirection::Input:
					// Input before output: error
					UE_LOG(LogAnimation, Error, TEXT("Term '%s' was used as an input before it was output"), *InName.ToString());
					return false;
				case EScheduleTermDirection::Output:
					return true;
				}

				return false;
			};

			if (UAnimNextScheduleEntry_Port* PortEntry = Cast<UAnimNextScheduleEntry_Port>(Entry))
			{
				bool bValid = true;

				if(PortEntry->Port == nullptr)
				{
					UE_LOG(LogAnimation, Error, TEXT("AnimNext: Invalid port class found"));
					bValid = false;
				}
				else
				{
					UAnimNextSchedulePort* CDO = PortEntry->Port->GetDefaultObject<UAnimNextSchedulePort>();
					check(CDO);

					TConstArrayView<FScheduleTerm> Terms = CDO->GetTerms();
					if(PortEntry->Terms.Num() != Terms.Num())
					{
						UE_LOG(LogAnimation, Error, TEXT("AnimNext: Incorrect term count for port: %d"), PortEntry->Terms.Num());
						bValid = false;
					}

					for(int32 TermIndex = 0; TermIndex < PortEntry->Terms.Num(); ++TermIndex)
					{
						FName TermName = PortEntry->Terms[TermIndex].Name;
						if(!PortEntry->Terms[TermIndex].Type.IsValid())
						{
							UE_LOG(LogAnimation, Error, TEXT("AnimNext: Invalid type when processing port term, ignored: '%s'"), *TermName.ToString());
							bValid = false;
						}
						else
						{
							const uint32* ExistingIntermediateIndexPtr = IntermediateMap.Find(TermName);
							if(ExistingIntermediateIndexPtr != nullptr)
							{
								const FAnimNextScheduleEntryTerm& IntermediateTerm = IntermediateTerms[*ExistingIntermediateIndexPtr];
								if(IntermediateTerm.Type != Terms[TermIndex].GetType())
								{
									UE_LOG(LogAnimation, Error, TEXT("AnimNext: Mismatched types when processing port term, ignored: '%s'"), *TermName.ToString());
									bValid = false;
								}

								if(!CheckTermDirectionCompatibility(TermName, IntermediateTerm.Direction, Terms[TermIndex].Direction))
								{
									bValid = false;
								}
							}
						}
					}
				}

				if(bValid)
				{
					EmitPrerequisite();

					FAnimNextSchedulePortTask PortTask;
					PortTask.TaskIndex = Ports.Num();
					PortTask.ParamScopeIndex = ParentScopeIndex;
					PortTask.Port = PortEntry->Port;

					for(int32 TermIndex = 0; TermIndex < PortEntry->Terms.Num(); ++TermIndex)
					{
						FName TermName = PortEntry->Terms[TermIndex].Name;
						const uint32* ExistingIntermediateIndexPtr = IntermediateMap.Find(TermName);
						if(ExistingIntermediateIndexPtr == nullptr)
						{
							uint32 IntermediateIndex = IntermediateTerms.Emplace(TermName, PortEntry->Terms[TermIndex].Type, PortEntry->Terms[TermIndex].Direction);
							IntermediateMap.Add(TermName, IntermediateIndex);
							PortTask.Terms.Add(IntermediateIndex);
						}
						else
						{
							PortTask.Terms.Add(*ExistingIntermediateIndexPtr);
						}
					}

					int32 PortIndex = Ports.Add(PortTask);

					NumTickFunctions++;

					Emit(EAnimNextScheduleScheduleOpcode::RunPort, PortIndex);
				}
			}
			else if (UAnimNextScheduleEntry_AnimNextGraph* GraphEntry = Cast<UAnimNextScheduleEntry_AnimNextGraph>(Entry))
			{
				bool bValid = true;

				if(GraphEntry->Graph == nullptr && GraphEntry->DynamicGraph == NAME_None)
				{
					UE_LOG(LogAnimation, Error, TEXT("AnimNext: Invalid graph or no parameter supplied"));
					bValid = false;
				}
				else if(GraphEntry->Graph != nullptr)
				{
					TConstArrayView<FScheduleTerm> Terms = GraphEntry->Graph->GetTerms();
					if(GraphEntry->Terms.Num() != Terms.Num())
					{
						UE_LOG(LogAnimation, Error, TEXT("AnimNext: Incorrect term count for graph: %d"), GraphEntry->Terms.Num());
						bValid = false;
					}
					else
					{
						// Validate graph terms match schedule-expected terms
						for(int32 TermIndex = 0; TermIndex < GraphEntry->Terms.Num(); ++TermIndex)
						{
							FName TermName = GraphEntry->Terms[TermIndex].Name;
							if(Terms[TermIndex].Direction != GraphEntry->Terms[TermIndex].Direction)
							{
								UE_LOG(LogAnimation, Error, TEXT("AnimNext: Mismatched direction when processing graph term, ignored: '%s'"), *TermName.ToString());
								bValid = false;
							}
							
							if(Terms[TermIndex].GetType() != GraphEntry->Terms[TermIndex].Type)
							{
								UE_LOG(LogAnimation, Error, TEXT("AnimNext: Mismatched types when processing graph term, ignored: '%s'"), *TermName.ToString());
								bValid = false;
							}
						}
					}
				}

				// Validate terms and check against priors
				for(int32 TermIndex = 0; TermIndex < GraphEntry->Terms.Num(); ++TermIndex)
				{
					FName TermName = GraphEntry->Terms[TermIndex].Name;
					if(!GraphEntry->Terms[TermIndex].Type.IsValid())
					{
						UE_LOG(LogAnimation, Error, TEXT("AnimNext: Invalid type when processing graph term, ignored: '%s'"), *TermName.ToString());
						bValid = false;
					}
					else
					{
						const uint32* ExistingIntermediateIndexPtr = IntermediateMap.Find(TermName);
						if(ExistingIntermediateIndexPtr != nullptr)
						{
							const FAnimNextScheduleEntryTerm& IntermediateTerm = IntermediateTerms[*ExistingIntermediateIndexPtr];
							if(IntermediateTerm.Type != GraphEntry->Terms[TermIndex].Type)
							{
								UE_LOG(LogAnimation, Error, TEXT("AnimNext: Mismatched types when processing graph term, ignored: '%s'"), *TermName.ToString());
								bValid = false;
							}
								
							if(!CheckTermDirectionCompatibility(TermName, IntermediateTerm.Direction, GraphEntry->Terms[TermIndex].Direction))
							{
								bValid = false;
							}
						}
					}
				}

				if(bValid)
				{
					EmitPrerequisite();

					FAnimNextScheduleGraphTask Task;
					Task.TaskIndex = Tasks.Num();
					Task.ParamScopeIndex = NumParameterScopes++;
					Task.ParamParentScopeIndex = ParentScopeIndex;
					Task.EntryPoint = GraphEntry->EntryPoint;
					Task.Graph = GraphEntry->Graph;
					Task.DynamicGraph = GraphEntry->DynamicGraph;

					for(int32 TermIndex = 0; TermIndex < GraphEntry->Terms.Num(); ++TermIndex)
					{
						FName TermName = GraphEntry->Terms[TermIndex].Name;
						const uint32* ExistingIntermediateIndexPtr = IntermediateMap.Find(TermName);
						if(ExistingIntermediateIndexPtr == nullptr)
						{
							uint32 IntermediateIndex = IntermediateTerms.Emplace(TermName, GraphEntry->Terms[TermIndex].Type, GraphEntry->Terms[TermIndex].Direction);
							IntermediateMap.Add(TermName, IntermediateIndex);
							Task.Terms.Add(IntermediateIndex);
						}
						else
						{
							Task.Terms.Add(*ExistingIntermediateIndexPtr);
						}
					}

					int32 TaskIndex = Tasks.Add(Task);

					NumTickFunctions++;

					Emit(EAnimNextScheduleScheduleOpcode::RunTask, TaskIndex);
				}
			}
			else if (UAnimNextScheduleEntry_ExternalTask* ExternalTaskEntry = Cast<UAnimNextScheduleEntry_ExternalTask>(Entry))
			{
				EmitPrerequisite();

				FAnimNextScheduleExternalTask ExternalTask;
				ExternalTask.TaskIndex = ExternalTasks.Num();
				ExternalTask.ParamScopeIndex = NumParameterScopes++;
				ExternalTask.ParamParentScopeIndex = ParentScopeIndex;
				ExternalTask.TickFunction = ExternalTaskEntry->TickFunction;
				ExternalTask.Object = ExternalTaskEntry->Object;
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
				ParamScopeEntryTask.Scope = ParamScopeTaskEntry->Scope;
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
				ParamScopeExitTask.Scope = ParamScopeTaskEntry->Scope;
				int32 ParamScopeExitTaskIndex = ParamScopeExitTasks.Add(ParamScopeExitTask);

				Emit(EAnimNextScheduleScheduleOpcode::RunParamScopeExit, ParamScopeExitTaskIndex);
				NumTickFunctions++;
			}
		}
	};

	EmitEntries(Entries);

	Emit(EAnimNextScheduleScheduleOpcode::Exit);

	// Process intermediates
	if(IntermediateMap.Num() > 0)
	{
		check(IntermediateMap.Num() == IntermediateTerms.Num());
		
		TArray<FPropertyBagPropertyDesc> PropertyDescs;
		PropertyDescs.Reserve(IntermediateTerms.Num());
		 
		for(const TPair<FName, uint32>& IntermediatePair : IntermediateMap)
		{
			const FAnimNextParamType& IntermediateType = IntermediateTerms[IntermediatePair.Value].Type;
			check(IntermediateType.IsValid());
			PropertyDescs.Emplace(IntermediatePair.Key, IntermediateType.GetContainerType(), IntermediateType.GetValueType(), IntermediateType.GetValueTypeObject());
		}

		IntermediatesData.AddProperties(PropertyDescs);
	}
}

#endif // #if WITH_EDITOR