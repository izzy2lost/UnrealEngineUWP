// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"

class UMovieSceneAnimatorSection;

struct FMovieSceneAnimatorSectionData
{
	/** Channel used to push sequencer time to */
	uint8 Channel = 0;

	/** Whether to use the section time or track time (sequencer time) */
	bool bUseSectionTime = true;

	/** The section object to get easing from */
	const UMovieSceneAnimatorSection* Section = nullptr;
};
