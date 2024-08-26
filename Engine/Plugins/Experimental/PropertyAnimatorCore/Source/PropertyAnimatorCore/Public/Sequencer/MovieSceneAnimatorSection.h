// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieSceneAnimatorTypes.h"
#include "MovieSceneSection.h"
#include "MovieSceneAnimatorSection.generated.h"

/** Movie scene section for a sequencer animator channel */
UCLASS(MinimalAPI)
class UMovieSceneAnimatorSection : public UMovieSceneSection
{
	GENERATED_BODY()

public:
	UMovieSceneAnimatorSection();

	PROPERTYANIMATORCORE_API void SetChannel(uint8 InChannel);

	uint8 GetChannel() const
	{
		return Channel;
	}

	PROPERTYANIMATORCORE_API void SetUseSectionTime(bool bInUse);

	bool GetUseSectionTime() const
	{
		return bUseSectionTime;
	}

protected:
	/** Channel used to push sequencer time to */
	UPROPERTY(EditAnywhere, Category="Animator")
	uint8 Channel = 0;

	/** Whether to use the section time (playback range) or track time (sequencer time) */
	UPROPERTY(EditAnywhere, Category="Animator")
	bool bUseSectionTime = true;
};
