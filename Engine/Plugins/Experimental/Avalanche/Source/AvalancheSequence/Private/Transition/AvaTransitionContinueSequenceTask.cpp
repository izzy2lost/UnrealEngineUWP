// Copyright Epic Games, Inc. All Rights Reserved.

#include "Transition/AvaTransitionContinueSequenceTask.h"
#include "AvaSequencePlaybackObject.h"
#include "StateTreeExecutionContext.h"

TArray<UAvaSequencePlayer*> FAvaTransitionContinueSequenceTask::ExecuteSequenceTask(FStateTreeExecutionContext& InContext) const
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
		return PlaybackObject->ContinueSequencesByLabel(InstanceData.SequenceName);

	case EAvaTransitionSequenceQueryType::Tag:
		if (const FAvaTag* Tag = InstanceData.SequenceTag.GetTag())
		{
			return PlaybackObject->ContinueSequencesByTag(*Tag, InstanceData.bPerformExactMatch);
		}
		return TArray<UAvaSequencePlayer*>();
	}

	checkNoEntry();
	return TArray<UAvaSequencePlayer*>();
}
