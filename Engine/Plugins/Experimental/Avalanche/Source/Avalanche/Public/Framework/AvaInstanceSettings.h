// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaInstanceSettings.generated.h"

USTRUCT()
struct FAvaSynchronizedEventsFeatureSelection
{
	GENERATED_BODY()
	
	/**
	 * Select the implementation for synchronizing events.
	 * "Default" will select the most appropriate implementation available.
	 */
	UPROPERTY(Config, EditAnywhere, Category = Settings)
	FString Implementation;
};

/**
 * Settings applied when instancing a Motion Design Asset for playback.
 */
USTRUCT()
struct FAvaInstanceSettings
{
	GENERATED_BODY()

	/** Enable loading dependent levels as sub-playables. */
	UPROPERTY(config, EditAnywhere, Category = Settings)
	bool bEnableLoadSubPlayables = true;

	/**
	 * For default playable transitions (when there is no transition tree),
	 * wait for the sequences to finish before ending the transition.
	 */
	UPROPERTY(config, EditAnywhere, Category = Settings)
	bool bDefaultPlayableTransitionWaitForSequences = false;

	/**
	 * Select the implementation for synchronizing events.
	 */
	UPROPERTY(Config, EditAnywhere, Category = Settings)
	FAvaSynchronizedEventsFeatureSelection SynchronizedEventsFeature;
};
