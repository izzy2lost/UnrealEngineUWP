// Copyright Epic Games, Inc. All Rights Reserved.

#include "Sequencer/MovieSceneAnimatorSection.h"

UMovieSceneAnimatorSection::UMovieSceneAnimatorSection()
	: UMovieSceneSection()
{
	bSupportsInfiniteRange = true;
}

void UMovieSceneAnimatorSection::SetChannel(uint8 InChannel)
{
	Channel = InChannel;
}
