// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tasks/AvaTransitionDelayTask.h"
#include "AvaTransitionUtils.h"
#include "StateTreeExecutionContext.h"

#define LOCTEXT_NAMESPACE "AvaTransitionDelayTask"

#if WITH_EDITOR
FText FAvaTransitionDelayTask::GetDescription(const FGuid& InId, FStateTreeDataView InInstanceDataView, const IStateTreeBindingLookup& InBindingLookup, EStateTreeNodeFormatting InFormatting) const
{
	const FInstanceDataType& InstanceData = InInstanceDataView.Get<FInstanceDataType>();
	return FText::Format(LOCTEXT("TaskDescription", "Delay {0} seconds"), FText::AsNumber(InstanceData.Duration));
}
#endif

void FAvaTransitionDelayTask::PostLoad(FStateTreeDataView InInstanceDataView)
{
	Super::PostLoad(InInstanceDataView);

	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	if (Duration_DEPRECATED >= 0.f)
	{
		if (FInstanceDataType* InstanceData = UE::AvaTransition::TryGetInstanceData(*this, InInstanceDataView))
		{
			InstanceData->Duration = Duration_DEPRECATED;
		}
	}
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

EStateTreeRunStatus FAvaTransitionDelayTask::EnterState(FStateTreeExecutionContext& InContext, const FStateTreeTransitionResult& InTransition) const
{
	FInstanceDataType& InstanceData = InContext.GetInstanceData(*this);
	InstanceData.RemainingTime = InstanceData.Duration;

	if (InstanceData.RemainingTime <= 0.f)
	{
		return EStateTreeRunStatus::Succeeded;
	}

	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FAvaTransitionDelayTask::Tick(FStateTreeExecutionContext& InContext, const float InDeltaTime) const
{
	FInstanceDataType& InstanceData = InContext.GetInstanceData(*this);

	InstanceData.RemainingTime -= InDeltaTime;

	if (InstanceData.RemainingTime <= 0.f)
	{
		return EStateTreeRunStatus::Succeeded;
	}
	return EStateTreeRunStatus::Running;
}

#undef LOCTEXT_NAMESPACE
