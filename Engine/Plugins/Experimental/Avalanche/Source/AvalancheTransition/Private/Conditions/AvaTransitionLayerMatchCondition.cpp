// Copyright Epic Games, Inc. All Rights Reserved.

#include "Conditions/AvaTransitionLayerMatchCondition.h"
#include "AvaTransitionContext.h"
#include "StateTreeExecutionContext.h"
#include "Behavior/AvaTransitionBehaviorInstance.h"

#define LOCTEXT_NAMESPACE "AvaTransitionLayerMatchCondition"

#if WITH_EDITOR
FText FAvaTransitionLayerMatchCondition::GetDescription(const FGuid& InId, FStateTreeDataView InInstanceDataView, const IStateTreeBindingLookup& InBindingLookup, EStateTreeNodeFormatting InFormatting) const
{
	const FInstanceDataType& InstanceData = InInstanceDataView.Get<FInstanceDataType>();
	return FText::Format(LOCTEXT("ConditionDescription", "scenes transitioning in {0}"), GetLayerQueryText(InstanceData));
}
#endif

bool FAvaTransitionLayerMatchCondition::TestCondition(FStateTreeExecutionContext& InContext) const
{
	const FAvaTransitionContext& TransitionContext = InContext.GetExternalData(TransitionContextHandle);
	const FInstanceDataType& InstanceData = InContext.GetInstanceData(*this);

	if (InstanceData.LayerType == EAvaTransitionLayerCompareType::Same && TransitionContext.GetTransitionType() == EAvaTransitionType::In)
	{
		return true;
	}

	TArray<const FAvaTransitionBehaviorInstance*> BehaviorInstances = QueryBehaviorInstances(InContext);
	return BehaviorInstances.ContainsByPredicate([](const FAvaTransitionBehaviorInstance* InInstance)
		{
			return InInstance && InInstance->GetTransitionType() == EAvaTransitionType::In;
		});
}

#undef LOCTEXT_NAMESPACE
