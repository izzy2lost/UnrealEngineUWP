// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tasks/AvaTransitionWaitForLayerTask.h"
#include "AvaTransitionLayerUtils.h"
#include "Behavior/AvaTransitionBehaviorInstance.h"
#include "StateTreeExecutionContext.h"

#define LOCTEXT_NAMESPACE "AvaTransitionWaitForLayerTask"

#if WITH_EDITOR
FText FAvaTransitionWaitForLayerTask::GetDescription(const FGuid& InId, FStateTreeDataView InInstanceDataView, const IStateTreeBindingLookup& InBindingLookup, EStateTreeNodeFormatting InFormatting) const
{
	const FInstanceDataType& InstanceData = InInstanceDataView.Get<FInstanceDataType>();
	return FText::Format(LOCTEXT("TaskDescription", "Wait for others in {0} to finish"), GetLayerQueryText(InstanceData));
}
#endif

EStateTreeRunStatus FAvaTransitionWaitForLayerTask::EnterState(FStateTreeExecutionContext& InContext, const FStateTreeTransitionResult& InTransition) const
{
	return QueryStatus(InContext);
}

EStateTreeRunStatus FAvaTransitionWaitForLayerTask::Tick(FStateTreeExecutionContext& InContext, const float InDeltaTime) const
{
	return QueryStatus(InContext);
}

EStateTreeRunStatus FAvaTransitionWaitForLayerTask::QueryStatus(FStateTreeExecutionContext& InContext) const
{
	TArray<const FAvaTransitionBehaviorInstance*> BehaviorInstances = QueryBehaviorInstances(InContext);

	bool bIsLayerRunning = BehaviorInstances.ContainsByPredicate(
		[](const FAvaTransitionBehaviorInstance* InInstance)
		{
			check(InInstance);
			return InInstance->IsRunning();
		});

	if (bIsLayerRunning)
	{
		return EStateTreeRunStatus::Running;
	}

	return EStateTreeRunStatus::Succeeded;
}

#undef LOCTEXT_NAMESPACE
