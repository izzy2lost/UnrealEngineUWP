// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tasks/AvaTransitionLayerTask.h"
#include "AvaTransitionContext.h"
#include "AvaTransitionLayerUtils.h"
#include "AvaTransitionSubsystem.h"
#include "AvaTransitionUtils.h"
#include "StateTreeExecutionContext.h"

#define LOCTEXT_NAMESPACE "AvaTransitionLayerTask"

void FAvaTransitionLayerTask::PostLoad(FStateTreeDataView InInstanceDataView)
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

FText FAvaTransitionLayerTask::GetLayerQueryText(const FInstanceDataType& InInstanceData) const
{
	return FAvaTransitionLayerUtils::GetLayerQueryText(InInstanceData.LayerType, InInstanceData.SpecificLayer.ToName());
}

TArray<const FAvaTransitionBehaviorInstance*> FAvaTransitionLayerTask::QueryBehaviorInstances(const FStateTreeExecutionContext& InContext) const
{
	const FInstanceDataType& InstanceData = InContext.GetInstanceData(*this);

	UAvaTransitionSubsystem& TransitionSubsystem   = InContext.GetExternalData(TransitionSubsystemHandle);
	const FAvaTransitionContext& TransitionContext = InContext.GetExternalData(TransitionContextHandle);
	const FAvaTransitionLayerComparator Comparator = FAvaTransitionLayerUtils::BuildComparator(TransitionContext, InstanceData.LayerType, InstanceData.SpecificLayer);

	return FAvaTransitionLayerUtils::QueryBehaviorInstances(TransitionSubsystem, Comparator);
}

#undef LOCTEXT_NAMESPACE
