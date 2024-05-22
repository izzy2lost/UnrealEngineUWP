// Copyright Epic Games, Inc. All Rights Reserved.

#include "Conditions/AvaTransitionStateMatchCondition.h"
#include "AvaTransitionUtils.h"
#include "Behavior/AvaTransitionBehaviorInstance.h"
#include "StateTreeExecutionContext.h"

#define LOCTEXT_NAMESPACE "AvaTransitionSceneMatchCondition"

#if WITH_EDITOR
FText FAvaTransitionStateMatchCondition::GetDescription(const FGuid& InId, FStateTreeDataView InInstanceDataView, const IStateTreeBindingLookup& InBindingLookup, EStateTreeNodeFormatting InFormatting) const
{
	const FInstanceDataType& InstanceData = InInstanceDataView.Get<FInstanceDataType>();

	return FText::Format(LOCTEXT("ConditionDescription", "{0} scene in {1}")
		, UEnum::GetDisplayValueAsText(InstanceData.TransitionState).ToLower()
		, GetLayerQueryText(InstanceData));
}
#endif

void FAvaTransitionStateMatchCondition::PostLoad(FStateTreeDataView InInstanceDataView)
{
	Super::PostLoad(InInstanceDataView);

	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	if (TransitionState_DEPRECATED != EAvaTransitionRunState::Unknown)
	{
		if (FInstanceDataType* InstanceData = UE::AvaTransition::TryGetInstanceData(*this, InInstanceDataView))
		{
			InstanceData->TransitionState = TransitionState_DEPRECATED;
		}
	}
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

bool FAvaTransitionStateMatchCondition::TestCondition(FStateTreeExecutionContext& InContext) const
{
	TArray<const FAvaTransitionBehaviorInstance*> BehaviorInstances = QueryBehaviorInstances(InContext);

	bool bIsLayerRunning = BehaviorInstances.ContainsByPredicate(
		[](const FAvaTransitionBehaviorInstance* InInstance)
		{
			check(InInstance);
			return InInstance->IsRunning();
		});

	const FInstanceDataType& InstanceData = InContext.GetInstanceData(*this);

	switch (InstanceData.TransitionState)
	{
	case EAvaTransitionRunState::Running:
		return bIsLayerRunning;

	case EAvaTransitionRunState::Finished:
		return !bIsLayerRunning;
	}

	checkNoEntry();
	return false;
}

#undef LOCTEXT_NAMESPACE
