// Copyright Epic Games, Inc. All Rights Reserved.

#include "Transition/Tasks/AvaSceneAddTagAttributeTask.h"
#include "AvaSceneState.h"
#include "IAvaSceneInterface.h"
#include "StateTreeExecutionContext.h"

#define LOCTEXT_NAMESPACE "AvaSceneAddTagAttributeTask"

#if WITH_EDITOR
FText FAvaSceneAddTagAttributeTask::GetDescription(const FGuid& InId, FStateTreeDataView InInstanceDataView, const IStateTreeBindingLookup& InBindingLookup, EStateTreeNodeFormatting InFormatting) const
{
	const FInstanceDataType& InstanceData = InInstanceDataView.Get<FInstanceDataType>();
	return FText::Format(LOCTEXT("TaskDescription", "Add '{0}' tag attribute to this scene"), FText::FromName(InstanceData.TagAttribute.ToName()));
}
#endif

EStateTreeRunStatus FAvaSceneAddTagAttributeTask::EnterState(FStateTreeExecutionContext& InContext, const FStateTreeTransitionResult& InTransition) const
{
	IAvaSceneInterface* Scene = GetScene(InContext);
	if (!Scene)
	{
		return EStateTreeRunStatus::Failed;
	}

	UAvaSceneState* SceneState = Scene->GetSceneState();
	if (!SceneState)
	{
		return EStateTreeRunStatus::Failed;
	}

	const FInstanceDataType& InstanceData = InContext.GetInstanceData(*this);
	if (SceneState->AddTagAttribute(InstanceData.TagAttribute))
	{
		return EStateTreeRunStatus::Succeeded;
	}

	return EStateTreeRunStatus::Failed;
}

#undef LOCTEXT_NAMESPACE
