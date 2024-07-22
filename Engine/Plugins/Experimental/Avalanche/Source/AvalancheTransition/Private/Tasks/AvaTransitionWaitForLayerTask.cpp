// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tasks/AvaTransitionWaitForLayerTask.h"
#include "AvaTransitionLayerUtils.h"
#include "Behavior/AvaTransitionBehaviorInstance.h"
#include "Engine/Level.h"
#include "Rendering/AvaTransitionRenderingSubsystem.h"
#include "StateTreeExecutionContext.h"
#include "StateTreeLinker.h"

#define LOCTEXT_NAMESPACE "AvaTransitionWaitForLayerTask"

#if WITH_EDITOR
FText FAvaTransitionWaitForLayerTask::GetDescription(const FGuid& InId, FStateTreeDataView InInstanceDataView, const IStateTreeBindingLookup& InBindingLookup, EStateTreeNodeFormatting InFormatting) const
{
	const FText LayerDesc = Super::GetDescription(InId, InInstanceDataView, InBindingLookup, InFormatting);

	return InFormatting == EStateTreeNodeFormatting::RichText
		? FText::Format(LOCTEXT("DescRich", "Wait <s>for others in</> {0} <s>to finish</>"), LayerDesc)
		: FText::Format(LOCTEXT("Desc", "Wait for others in {0} to finish"), LayerDesc);
}
#endif

bool FAvaTransitionWaitForLayerTask::Link(FStateTreeLinker& InLinker)
{
	Super::Link(InLinker);
	InLinker.LinkExternalData(RenderingSubsystemHandle);
	return true;
}

EStateTreeRunStatus FAvaTransitionWaitForLayerTask::EnterState(FStateTreeExecutionContext& InContext, const FStateTreeTransitionResult& InTransition) const
{
	return WaitForLayer(InContext);
}

EStateTreeRunStatus FAvaTransitionWaitForLayerTask::Tick(FStateTreeExecutionContext& InContext, const float InDeltaTime) const
{
	return WaitForLayer(InContext);
}

EStateTreeRunStatus FAvaTransitionWaitForLayerTask::WaitForLayer(FStateTreeExecutionContext& InContext) const
{
	TArray<const FAvaTransitionBehaviorInstance*> BehaviorInstances = QueryBehaviorInstances(InContext);

	bool bIsLayerRunning = BehaviorInstances.ContainsByPredicate(
		[](const FAvaTransitionBehaviorInstance* InInstance)
		{
			check(InInstance);
			return InInstance->IsRunning();
		});

	FAvaTransitionWaitForLayerTask::FInstanceDataType& InstanceData = InContext.GetInstanceData(*this);

	UAvaTransitionRenderingSubsystem& RenderingSubsystem = InContext.GetExternalData(RenderingSubsystemHandle);

	if (bIsLayerRunning)
	{
		// Hide Level Primitives while Waiting for other Layers
		if (InstanceData.bHideSceneWhileWaiting && InstanceData.HiddenLevel == nullptr)
		{
			const FAvaTransitionContext& TransitionContext = InContext.GetExternalData(TransitionContextHandle);
			if (const FAvaTransitionScene* TransitionScene = TransitionContext.GetTransitionScene())
			{
				InstanceData.HiddenLevel = TransitionScene->GetLevel();
				RenderingSubsystem.HideLevel(InstanceData.HiddenLevel);
			}
		}

		return EStateTreeRunStatus::Running;
	}

	// Restore Level Visibility
	RenderingSubsystem.ShowLevel(InstanceData.HiddenLevel);
	return EStateTreeRunStatus::Succeeded;
}

#undef LOCTEXT_NAMESPACE
