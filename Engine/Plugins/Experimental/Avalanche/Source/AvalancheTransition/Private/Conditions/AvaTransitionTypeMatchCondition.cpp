// Copyright Epic Games, Inc. All Rights Reserved.

#include "Conditions/AvaTransitionTypeMatchCondition.h"
#include "AvaTransitionContext.h"
#include "StateTreeExecutionContext.h"

#define LOCTEXT_NAMESPACE "AvaTransitionTypeMatchCondition"

#if WITH_EDITOR
FText FAvaTransitionTypeMatchCondition::GetDescription(const FGuid& InId, FStateTreeDataView InInstanceDataView, const IStateTreeBindingLookup& InBindingLookup, EStateTreeNodeFormatting InFormatting) const
{
	return FText::Format(LOCTEXT("ConditionDescription", "transitioning {0}")
		, UEnum::GetDisplayValueAsText(TransitionType).ToLower());
}
#endif

bool FAvaTransitionTypeMatchCondition::TestCondition(FStateTreeExecutionContext& InContext) const
{
	const FAvaTransitionContext& TransitionContext = InContext.GetExternalData(TransitionContextHandle);
	return TransitionContext.GetTransitionType() == TransitionType;
}

#undef LOCTEXT_NAMESPACE
