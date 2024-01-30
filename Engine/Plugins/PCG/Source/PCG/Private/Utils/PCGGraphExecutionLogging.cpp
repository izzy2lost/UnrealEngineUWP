// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGGraphExecutionLogging.h"

#include "PCGComponent.h"
#include "PCGContext.h"
#include "PCGModule.h"
#include "Graph/PCGGraphExecutor.h"

#include "GameFramework/Actor.h"
#include "HAL/IConsoleManager.h"

namespace PCGGraphExecutionLogging
{
#if WITH_EDITOR
	static TAutoConsoleVariable<bool> CVarGraphExecutionLoggingEnable(
		TEXT("pcg.GraphExecution.EnableLogging"),
		false,
		TEXT("Enables fine grained log of graph execution"));

	static TAutoConsoleVariable<bool> CVarGraphExecutionCullingLoggingEnable(
		TEXT("pcg.GraphExecution.EnableCullingLogging"),
		false,
		TEXT("Enables fine grained log of dynamic task culling during graph execution"));
#endif

	bool LogEnabled()
	{
#if WITH_EDITOR
		return CVarGraphExecutionLoggingEnable.GetValueOnAnyThread();
#else
		return false;
#endif
	}

	bool CullingLogEnabled()
	{
#if WITH_EDITOR
		return CVarGraphExecutionCullingLoggingEnable.GetValueOnAnyThread();
#else
		return false;
#endif
	}

	void LogGraphTask(FPCGTaskId TaskId, const FPCGGraphTask& Task, const TSet<FPCGTaskId>* SuccessorIds)
	{
#if WITH_EDITOR
		if (!LogEnabled())
		{
			return;
		}

		auto GenerateInputsString = [](const TArray<FPCGGraphTaskInput>& Inputs)
		{
			FString InputString;
			bool bFirstInput = true;

			for (const FPCGGraphTaskInput& Input : Inputs)
			{
				if (!bFirstInput)
				{
					InputString += TEXT(",");
				}
				bFirstInput = false;

				InputString += FString::Printf(TEXT("%u->'%s'"), Input.TaskId, Input.OutPin ? *Input.OutPin->Properties.Label.ToString() : TEXT(""));
			}

			return InputString;
		};

		FString SuccessorsString;
		if (SuccessorIds)
		{
			bool bFirstSuccessor = true;
			for (const FPCGTaskId& SuccessorId : *SuccessorIds)
			{
				SuccessorsString += bFirstSuccessor ? FString::Printf(TEXT("%u"), SuccessorId) : FString::Printf(TEXT(",%u"), SuccessorId);
				bFirstSuccessor = false;
			}
		}

		UE_LOG(LogPCG, Log, TEXT("\t\tID: %u\tParent: %u\tNode: %s\tInputs: %s\tPinDeps: %s\tSuccessors: %s"),
			TaskId,
			Task.ParentId != InvalidPCGTaskId ? Task.ParentId : 0,
			Task.Node ? (*Task.Node->GetNodeTitle(EPCGNodeTitleType::ListView).ToString()) : TEXT("NULL"),
			*GenerateInputsString(Task.Inputs),
			*Task.PinDependency.ToString(),
			*SuccessorsString
		);
#endif
	}

	void LogGraphTasks(const TMap<FPCGTaskId, FPCGGraphTask>& Tasks, const TMap<FPCGTaskId, TSet<FPCGTaskId>>* TaskSuccessors)
	{
#if WITH_EDITOR
		if (!LogEnabled())
		{
			return;
		}

		for (const TPair<FPCGTaskId, FPCGGraphTask>& TaskIdAndTask : Tasks)
		{
			PCGGraphExecutionLogging::LogGraphTask(TaskIdAndTask.Key, TaskIdAndTask.Value, TaskSuccessors ? TaskSuccessors->Find(TaskIdAndTask.Value.NodeId) : nullptr);
		}
#endif
	}

	void LogGraphTasks(const TArray<FPCGGraphTask>& Tasks)
	{
#if WITH_EDITOR
		if (!LogEnabled())
		{
			return;
		}

		for (const FPCGGraphTask& Task : Tasks)
		{
			LogGraphTask(Task.NodeId, Task);
		}
#endif
	}

	void LogGraphSchedule(const UPCGComponent* SourceComponent)
	{
#if WITH_EDITOR
		if (!LogEnabled())
		{
			return;
		}

		UE_LOG(LogPCG, Log, TEXT("[%s] --- SCHEDULE GRAPH ---"),
			(SourceComponent && SourceComponent->GetOwner()) ? *SourceComponent->GetOwner()->GetName() : TEXT("MISSINGCOMPONENT"));
#endif
	}

	void LogGraphPostSchedule(const TMap<FPCGTaskId, FPCGGraphTask>& Tasks, const TMap<FPCGTaskId, TSet<FPCGTaskId>>& TaskSuccessors)
	{
#if WITH_EDITOR
		if (!CullingLogEnabled())
		{
			return;
		}

		UE_LOG(LogPCG, Log, TEXT("POST SCHEDULE:"));

		LogGraphTasks(Tasks, &TaskSuccessors);
#endif
	}

	void LogGraphExecuteFrameFinished()
	{
#if WITH_EDITOR
		if (!LogEnabled())
		{
			return;
		}

		UE_LOG(LogPCG, Log, TEXT("--- FINISH FPCGGRAPHEXECUTOR::EXECUTE ---"));
#endif
	}

#if WITH_EDITOR
	FString GetPinsToDeactivateString(const TArray<FPCGPinId>& PinIdsToDeactivate)
	{
		FString PinIdsToDeactivateString;
		bool bFirst = true;

		for (const FPCGPinId& PinId : PinIdsToDeactivate)
		{
			const FPCGTaskId NodeId = PCGPinIdHelpers::GetNodeIdFromPinId(PinId);
			const uint64 PinIndex = PCGPinIdHelpers::GetPinIndexFromPinId(PinId);
			PinIdsToDeactivateString += bFirst ? FString::Printf(TEXT("%u_%u"), NodeId, PinIndex) : FString::Printf(TEXT(",%u_%u"), NodeId, PinIndex);
			bFirst = false;
		}

		return PinIdsToDeactivateString;
	}
#endif

	void LogTaskExecute(const FPCGGraphTask& Task)
	{
#if WITH_EDITOR
		if (!LogEnabled() || !Task.SourceComponent.Get())
		{
			return;
		}

		UE_LOG(LogPCG, Log, TEXT("         [%s] %s\t\tEXECUTE"),
			*Task.SourceComponent->GetOwner()->GetName(),
			*FString::Printf(TEXT("%u'%s'"), Task.NodeId, Task.Node ? *Task.Node->GetNodeTitle(EPCGNodeTitleType::ListView).ToString() : TEXT("")));
#endif
	}

	void LogTaskExecuteCachingDisabled(const FPCGGraphTask& Task)
	{
#if WITH_EDITOR
		if (!LogEnabled() || !Task.SourceComponent.Get())
		{
			return;
		}

		UE_LOG(LogPCG, Warning, TEXT("[%s] %s\t\tCACHING DISABLED"),
			*Task.SourceComponent->GetOwner()->GetName(),
			*FString::Printf(TEXT("%u'%s'"), Task.NodeId, Task.Node ? *Task.Node->GetNodeTitle(EPCGNodeTitleType::ListView).ToString() : TEXT("")));
#endif
	}

	void LogTaskExecuteOutputCRC(const FPCGGraphActiveTask& Task)
	{
#if WITH_EDITOR
		if (!LogEnabled() || !Task.Context || !Task.Context->SourceComponent.Get() || !Task.Context->SourceComponent->GetOwner())
		{
			return;
		}

		UE_LOG(LogPCG, Log, TEXT("         [%s] %s\t\tOUTPUT CRC %u"),
			*Task.Context->SourceComponent->GetOwner()->GetName(),
			*FString::Printf(TEXT("%u'%s'"), Task.NodeId, Task.Context->Node ? *Task.Context->Node->GetNodeTitle(EPCGNodeTitleType::ListView).ToString() : TEXT("")),
			Task.Context->OutputData.Crc.GetValue());
#endif
	}

	void LogTaskCullingBegin(FPCGTaskId CompletedTaskId, uint64 InactiveOutputPinBitmask, const TArray<FPCGPinId>& PinIdsToDeactivate)
	{
#if WITH_EDITOR
		if (!CullingLogEnabled())
		{
			return;
		}

		UE_LOG(LogPCG, Log, TEXT("BEGIN CullInactiveDownstreamNodes, CompletedTaskId: %u, InactiveOutputPinBitmask: %u, Deactivating pin IDs: %s"),
			CompletedTaskId, InactiveOutputPinBitmask, *GetPinsToDeactivateString(PinIdsToDeactivate));
#endif
	}

	void LogTaskCullingBeginLoop(FPCGTaskId PinTaskId, uint64 PinIndex, const TArray<FPCGPinId>& PinIdsToDeactivate)
	{
#if WITH_EDITOR
		if (!CullingLogEnabled())
		{
			return;
		}

		UE_LOG(LogPCG, Log, TEXT("LOOP: DEACTIVATE %u_%u, remaining IDs: %s"), PinTaskId, PinIndex, *GetPinsToDeactivateString(PinIdsToDeactivate));
#endif
	}

	void LogTaskCullingUpdatedPinDeps(FPCGTaskId TaskId, const FPCGPinDependencyExpression& PinDependency, bool bDependencyExpressionBecameFalse)
	{
#if WITH_EDITOR
		if (!CullingLogEnabled())
		{
			return;
		}

		UE_LOG(LogPCG, Log, TEXT("UPDATED PIN DEP EXPRESSION (task ID %u): %s"), TaskId, *PinDependency.ToString());

		if (bDependencyExpressionBecameFalse)
		{
			UE_LOG(LogPCG, Log, TEXT("CULL task ID %u"), TaskId);
		}
#endif
	}

	void LogGridLinkageTaskExecuteStore(const FPCGContext* InContext, EPCGHiGenGrid InGenerationGrid, int32 InFromGridSize, int32 InToGridSize, const FString& InResourcePath)
	{
#if WITH_EDITOR
		if (!LogEnabled())
		{
			return;
		}
		check(InContext);

		const FString OwnerName = InContext->SourceComponent.Get() ? (InContext->SourceComponent->GetOwner() ? InContext->SourceComponent->GetOwner()->GetActorLabel() : FString()) : FString();
		UE_LOG(LogPCG, Log, TEXT("[GRIDLINKING] [%s] STORE. GenerationGridSize=%d, FromGridSize=%d, ToGridSize=%d, Path=%s"), *OwnerName, PCGHiGenGrid::GridToGridSize(InGenerationGrid), InFromGridSize, InToGridSize, *InResourcePath);
#endif
	}

	void LogGridLinkageTaskExecuteRetrieve(const FPCGContext* InContext, EPCGHiGenGrid InGenerationGrid, int32 InFromGridSize, int32 InToGridSize, const FString& InResourcePath)
	{
#if WITH_EDITOR
		if (!LogEnabled())
		{
			return;
		}
		check(InContext);

		const FString OwnerName = InContext->SourceComponent.Get() ? (InContext->SourceComponent->GetOwner() ? InContext->SourceComponent->GetOwner()->GetActorLabel() : FString()) : FString();
		UE_LOG(LogPCG, Log, TEXT("[GRIDLINKING] [%s] RETRIEVE. GenerationGridSize=%d, FromGridSize=%d, ToGridSize=%d, Path=%s"), *OwnerName, PCGHiGenGrid::GridToGridSize(InGenerationGrid), InFromGridSize, InToGridSize, *InResourcePath);
#endif
	}

	void LogGridLinkageTaskExecuteRetrieveSuccess(const FPCGContext* InContext, const UPCGComponent* InComponent, const FString& InResourcePath, int32 InDataItemCount)
	{
#if WITH_EDITOR
		if (!LogEnabled())
		{
			return;
		}
		check(InContext);

		const FString OwnerName = InContext->SourceComponent.Get() ? (InContext->SourceComponent->GetOwner() ? InContext->SourceComponent->GetOwner()->GetActorLabel() : FString()) : FString();
		UE_LOG(LogPCG, Log, TEXT("[GRIDLINKING] [%s] RETRIEVE: SUCCESS. Component=%s Path=%s DataItems=%d"), *OwnerName, *InComponent->GetOwner()->GetActorLabel(), *InResourcePath, InDataItemCount);
#endif
	}

	void LogGridLinkageTaskExecuteRetrieveScheduleGraph(const FPCGContext* InContext, const UPCGComponent* InScheduledComponent, const FString& InResourcePath)
	{
#if WITH_EDITOR
		if (!LogEnabled())
		{
			return;
		}
		check(InContext);

		const FString OwnerName = (InContext->SourceComponent.Get() && InContext->SourceComponent->GetOwner()) ? InContext->SourceComponent->GetOwner()->GetActorLabel() : FString();
		const FString OtherOwnerName = (InScheduledComponent && InScheduledComponent->GetOwner()) ? InScheduledComponent->GetOwner()->GetActorLabel() : FString();
		UE_LOG(LogPCG, Log, TEXT("[GRIDLINKING] [%s] RETRIEVE: SCHEDULE GRAPH. Component=%s Path=%s"), *OwnerName, *OtherOwnerName, *InResourcePath);
#endif
	}

	void LogGridLinkageTaskExecuteRetrieveWaitOnScheduledGraph(const FPCGContext* InContext, const UPCGComponent* InWaitOnComponent, const FString& InResourcePath)
	{
#if WITH_EDITOR
		if (!LogEnabled())
		{
			return;
		}
		check(InContext);

		const FString OwnerName = (InContext->SourceComponent.Get() && InContext->SourceComponent->GetOwner()) ? InContext->SourceComponent->GetOwner()->GetActorLabel() : FString();
		const FString OtherOwnerName = (InWaitOnComponent && InWaitOnComponent->GetOwner()) ? InWaitOnComponent->GetOwner()->GetActorLabel() : FString();
		UE_LOG(LogPCG, Log, TEXT("[GRIDLINKING] [%s] RETRIEVE: WAIT FOR SCHEDULED GRAPH. Component=%s Path=%s"), *OwnerName, *OtherOwnerName, *InResourcePath);
#endif
	}
	
	void LogGridLinkageTaskExecuteRetrieveWakeUp(const FPCGContext* InContext, const UPCGComponent* InWokenBy)
	{
#if WITH_EDITOR
		if (!LogEnabled())
		{
			return;
		}
		check(InContext);

		const FString OwnerName = (InContext->SourceComponent.Get() && InContext->SourceComponent->GetOwner()) ? InContext->SourceComponent->GetOwner()->GetActorLabel() : FString();
		const FString OtherOwnerName = (InWokenBy && InWokenBy->GetOwner()) ? InWokenBy->GetOwner()->GetActorLabel() : FString();
		UE_LOG(LogPCG, Log, TEXT("[GRIDLINKING] [%s] RETRIEVE: WOKEN BY Component=%s"), *OwnerName, *OtherOwnerName);
#endif
	}

	void LogGridLinkageTaskExecuteRetrieveNoLocalComponent(const FPCGContext* InContext, const FString& InResourcePath)
	{
#if WITH_EDITOR
		if (!LogEnabled())
		{
			return;
		}
		check(InContext);

		const FString OwnerName = (InContext->SourceComponent.Get() && InContext->SourceComponent->GetOwner()) ? InContext->SourceComponent->GetOwner()->GetActorLabel() : FString();
		UE_LOG(LogPCG, Warning, TEXT("[GRIDLINKING] [%s] RETRIEVE: FAILED: No overlapping local component found. This may be expected. Path=%s"), *OwnerName, *InResourcePath);
#endif
	}

	void LogGridLinkageTaskExecuteRetrieveNoData(const FPCGContext* InContext, const UPCGComponent* InComponent, const FString& InResourcePath)
	{
#if WITH_EDITOR
		if (!LogEnabled())
		{
			return;
		}
		check(InContext);

		const FString OwnerName = (InContext->SourceComponent.Get() && InContext->SourceComponent->GetOwner()) ? InContext->SourceComponent->GetOwner()->GetActorLabel() : FString();
		const FString OtherOwnerName = (InComponent && InComponent->GetOwner()) ? InComponent->GetOwner()->GetActorLabel() : FString();
		UE_LOG(LogPCG, Warning, TEXT("[GRIDLINKING] [%s] RETRIEVE: FAILED: No data found on local component. Component=%s, Path=%s"), *OwnerName, *OtherOwnerName, *InResourcePath);
#endif
	}
}
