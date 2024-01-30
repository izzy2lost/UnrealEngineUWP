// Copyright Epic Games, Inc. All Rights Reserved.

#include "Transition/AvaTransitionInitializeSequence.h"
#include "AvaSequencePlaybackObject.h"

TArray<UAvaSequencePlayer*> FAvaTransitionInitializeSequence::ExecuteSequenceTask(FStateTreeExecutionContext& InContext) const
{
	IAvaSequencePlaybackObject* PlaybackObject = GetPlaybackObject(InContext);
	if (!PlaybackObject)
	{
		return TArray<UAvaSequencePlayer*>();
	}

	FAvaSequencePlayParams PlaySettings;
	PlaySettings.Start = PlaySettings.End = InitializeTime;
	PlaySettings.PlayMode = PlayMode;

	switch (QueryType)
	{
	case EAvaTransitionSequenceQueryType::Name:
		return PlaybackObject->PlaySequencesByLabel(SequenceName, PlaySettings);

	case EAvaTransitionSequenceQueryType::Tag:
		if (const FAvaTag* Tag = SequenceTag.GetTag())
		{
			return PlaybackObject->PlaySequencesByTag(*Tag, bPerformExactMatch, PlaySettings);
		}
		return TArray<UAvaSequencePlayer*>();
	}

	checkNoEntry();
	return TArray<UAvaSequencePlayer*>();
}
