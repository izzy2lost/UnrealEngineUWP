// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tasks/StateTreeRunParallelStateTreeTask.h"

#include "StateTreeExecutionContext.h"

FStateTreeRunParallelStateTreeTask::FStateTreeRunParallelStateTreeTask()
{
	bShouldCopyBoundPropertiesOnTick = false;
	bShouldCopyBoundPropertiesOnExitState = false;
}

EStateTreeRunStatus FStateTreeRunParallelStateTreeTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transitions) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	const FStateTreeReference& StateTreeToRun = GetStateTreeToRun(Context, InstanceData);
	if (!StateTreeToRun.IsValid())
	{
		return EStateTreeRunStatus::Failed;
	}

	InstanceData.RunningStateTree = StateTreeToRun.GetStateTree();
	FStateTreeExecutionContext ParallelTreeContext(Context, *InstanceData.RunningStateTree, InstanceData.TreeInstanceData);
	if (!ParallelTreeContext.IsValid())
	{
		return EStateTreeRunStatus::Failed;
	}

	return ParallelTreeContext.Start(&StateTreeToRun.GetParameters());
}

EStateTreeRunStatus FStateTreeRunParallelStateTreeTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (!InstanceData.RunningStateTree)
	{
		return EStateTreeRunStatus::Failed;
	}

	FStateTreeExecutionContext ParallelTreeContext(Context, *InstanceData.RunningStateTree, InstanceData.TreeInstanceData);
	if (!ParallelTreeContext.IsValid())
	{
		return EStateTreeRunStatus::Failed;
	}

	return ParallelTreeContext.Tick(DeltaTime);
}

void FStateTreeRunParallelStateTreeTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (!InstanceData.RunningStateTree)
	{
		return;
	}

	FStateTreeExecutionContext ParallelTreeContext(Context, *InstanceData.RunningStateTree, InstanceData.TreeInstanceData);
	if (!ParallelTreeContext.IsValid())
	{
		return;
	}

	ParallelTreeContext.Stop();
}

const FStateTreeReference& FStateTreeRunParallelStateTreeTask::GetStateTreeToRun(FStateTreeExecutionContext& Context, FInstanceDataType& InstanceData) const
{
	if (StateTreeOverrideTag.IsValid())
	{
		if (const FStateTreeReference* Override = Context.GetLinkedStateTreeOverrideForTag(StateTreeOverrideTag))
		{
			return *Override;
		}
	}

	return InstanceData.StateTree;
}

#if WITH_EDITOR
void FStateTreeRunParallelStateTreeTask::PostEditInstanceDataChangeChainProperty(const FPropertyChangedChainEvent& PropertyChangedEvent, FStateTreeDataView InstanceDataView)
{
	if (PropertyChangedEvent.Property && PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(FStateTreeRunParallelStateTreeTaskInstanceData, StateTree))
	{
		InstanceDataView.GetMutable<FInstanceDataType>().StateTree.SyncParameters();
	}
}

void FStateTreeRunParallelStateTreeTask::PostLoad(FStateTreeDataView InstanceDataView)
{
	if (FInstanceDataType* DataType = InstanceDataView.GetMutablePtr<FInstanceDataType>())
	{
		DataType->StateTree.SyncParameters();
	}
}
#endif // WITH_EDITOR