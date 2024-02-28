// Copyright Epic Games, Inc. All Rights Reserved.

#include "BehaviorTree/Tasks/BTTask_RunStateTree.h"

#include "BehaviorTree/GameplayStateTreeBTUtils.h"
#include "Components/StateTreeComponentSchema.h"
#include "StateTreeExecutionContext.h"

UBTTask_RunStateTree::UBTTask_RunStateTree(const FObjectInitializer& ObjectInitializer /*= FObjectInitializer::Get()*/)
	: Super(ObjectInitializer)
{
	INIT_TASK_NODE_NOTIFY_FLAGS();
	NodeName = TEXT("Run State Tree");
	bCreateNodeInstance = true;
	bTickIntervals = true;
}

EBTNodeResult::Type UBTTask_RunStateTree::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	if (StateTreeRef.IsValid())
	{
		FStateTreeExecutionContext Context(*OwnerComp.GetOwner(), *StateTreeRef.GetStateTree(), InstanceData);
		if (UStateTreeComponentSchema::SetContextRequirements(OwnerComp, Context))
		{
			const EStateTreeRunStatus StartStatus = Context.Start(&StateTreeRef.GetParameters());
			return GameplayStateTreeBTUtils::StateTreeRunStatusToBTNodeResult(StartStatus);
		}
	}

	return EBTNodeResult::Failed;
}

void UBTTask_RunStateTree::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	if (StateTreeRef.IsValid())
	{
		FStateTreeExecutionContext Context(*OwnerComp.GetOwner(), *StateTreeRef.GetStateTree(), InstanceData);
		if (UStateTreeComponentSchema::SetContextRequirements(OwnerComp, Context))
		{
			const EStateTreeRunStatus TickStatus = Context.Tick(DeltaSeconds);
			if (TickStatus != EStateTreeRunStatus::Running)
			{
				FinishLatentTask(OwnerComp, GameplayStateTreeBTUtils::StateTreeRunStatusToBTNodeResult(TickStatus));
			}
			else
			{
				SetNextTickTime(NodeMemory, FMath::Max(0.f, Interval + FMath::FRandRange(-RandomDeviation, RandomDeviation)));
			}
			return;
		}
	}

	FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
}

void UBTTask_RunStateTree::OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTNodeResult::Type TaskResult)
{
	if (StateTreeRef.IsValid())
	{
		FStateTreeExecutionContext Context(*OwnerComp.GetOwner(), *StateTreeRef.GetStateTree(), InstanceData);
		if(UStateTreeComponentSchema::SetContextRequirements(OwnerComp, Context))
		{
			Context.Stop();
		}
	}
}
