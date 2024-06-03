// Copyright Epic Games, Inc. All Rights Reserved.

#include "Transition/AvaTransitionWaitForSequenceTask.h"
#include "AvaSequencePlaybackObject.h"
#include "AvaSequencePlayer.h"
#include "StateTreeExecutionContext.h"

TArray<UAvaSequencePlayer*> FAvaTransitionWaitForSequenceTask::ExecuteSequenceTask(FStateTreeExecutionContext& InContext) const
{
	IAvaSequencePlaybackObject* PlaybackObject = GetPlaybackObject(InContext);
	if (!PlaybackObject)
	{
		return TArray<UAvaSequencePlayer*>();
	}

	const FInstanceDataType& InstanceData = InContext.GetInstanceData(*this);

	switch (InstanceData.QueryType)
	{
	case EAvaTransitionSequenceQueryType::Name:
		return PlaybackObject->GetSequencePlayersByLabel(InstanceData.SequenceName);

	case EAvaTransitionSequenceQueryType::Tag:
		return PlaybackObject->GetSequencePlayersByTag(InstanceData.SequenceTag, InstanceData.bPerformExactMatch);
	}

	checkNoEntry();
	return TArray<UAvaSequencePlayer*>();
}
