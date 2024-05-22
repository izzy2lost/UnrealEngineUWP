// Copyright Epic Games, Inc. All Rights Reserved.

#include "Conditions/AvaTransitionLayerCondition.h"
#include "AvaTransitionContext.h"
#include "AvaTransitionLayerUtils.h"
#include "AvaTransitionSubsystem.h"
#include "AvaTransitionUtils.h"
#include "StateTreeExecutionContext.h"

#define LOCTEXT_NAMESPACE "AvaTransitionLayerCondition"

void FAvaTransitionLayerCondition::PostLoad(FStateTreeDataView InInstanceDataView)
{
	Super::PostLoad(InInstanceDataView);

	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	if (LayerType_DEPRECATED != EAvaTransitionLayerCompareType::None)
	{
		if (FInstanceDataType* InstanceData = UE::AvaTransition::TryGetInstanceData(*this, InInstanceDataView))
		{
			InstanceData->LayerType     = LayerType_DEPRECATED;
			InstanceData->SpecificLayer = SpecificLayer_DEPRECATED;
		}
	}
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

TArray<const FAvaTransitionBehaviorInstance*> FAvaTransitionLayerCondition::QueryBehaviorInstances(const FStateTreeExecutionContext& InContext) const
{
	const FInstanceDataType& InstanceData = InContext.GetInstanceData(*this);

	UAvaTransitionSubsystem& TransitionSubsystem   = InContext.GetExternalData(TransitionSubsystemHandle);
	const FAvaTransitionContext& TransitionContext = InContext.GetExternalData(TransitionContextHandle);
	const FAvaTransitionLayerComparator Comparator = FAvaTransitionLayerUtils::BuildComparator(TransitionContext, InstanceData.LayerType, InstanceData.SpecificLayer);

	return FAvaTransitionLayerUtils::QueryBehaviorInstances(TransitionSubsystem, Comparator);
}

FText FAvaTransitionLayerCondition::GetLayerQueryText(const FInstanceDataType& InInstanceData) const
{
	return FAvaTransitionLayerUtils::GetLayerQueryText(InInstanceData.LayerType, InInstanceData.SpecificLayer.ToName());
}

#undef LOCTEXT_NAMESPACE
