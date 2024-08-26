// Copyright Epic Games, Inc. All Rights Reserved.

#include "Sequencer/MovieSceneAnimatorSection.h"

UMovieSceneAnimatorSection::UMovieSceneAnimatorSection()
	: UMovieSceneSection()
{
	bSupportsInfiniteRange = true;
	EvalOptions.CompletionMode = EMovieSceneCompletionMode::RestoreState;
}

void UMovieSceneAnimatorSection::SetChannel(uint8 InChannel)
{
	Channel = InChannel;
}

void UMovieSceneAnimatorSection::SetUseSectionTime(bool bInUse)
{
	bUseSectionTime = bInUse;
}
