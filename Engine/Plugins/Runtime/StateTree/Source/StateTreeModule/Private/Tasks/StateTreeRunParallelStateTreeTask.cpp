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
	if (!InstanceData.StateTree.IsValid())
	{
		return EStateTreeRunStatus::Failed;
	}

	FStateTreeExecutionContext ParallelTreeContext(Context, *InstanceData.StateTree.GetStateTree(), InstanceData.TreeInstanceData);
	if (!ParallelTreeContext.IsValid())
	{
		return EStateTreeRunStatus::Failed;
	}

	return ParallelTreeContext.Start(&InstanceData.StateTree.GetParameters());
}

EStateTreeRunStatus FStateTreeRunParallelStateTreeTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (!InstanceData.StateTree.IsValid())
	{
		return EStateTreeRunStatus::Failed;
	}

	FStateTreeExecutionContext ParallelTreeContext(Context, *InstanceData.StateTree.GetStateTree(), InstanceData.TreeInstanceData);
	if (!ParallelTreeContext.IsValid())
	{
		return EStateTreeRunStatus::Failed;
	}

	return ParallelTreeContext.Tick(DeltaTime);
}

void FStateTreeRunParallelStateTreeTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (!InstanceData.StateTree.IsValid())
	{
		return;
	}

	FStateTreeExecutionContext ParallelTreeContext(Context, *InstanceData.StateTree.GetStateTree(), InstanceData.TreeInstanceData);
	if (!ParallelTreeContext.IsValid())
	{
		return;
	}

	ParallelTreeContext.Stop();
}

#if WITH_EDITOR
void FStateTreeRunParallelStateTreeTask::PostEditInstanceDataChangeChainProperty(const FPropertyChangedChainEvent& PropertyChangedEvent, FStateTreeDataView InstanceDataView)
{
	if (PropertyChangedEvent.Property && PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(FStateTreeRunParallelStateTreeTaskInstanceData, StateTree))
	{
		InstanceDataView.GetMutable<FInstanceDataType>().StateTree.SyncParameters();
	}
}
#endif // WITH_EDITOR