// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvalancheInstanceSettings.generated.h"

/**
 * Settings applied when instancing an avalanche blueprint for playback.
 */
USTRUCT()
struct FAvalancheInstanceSettings
{
	GENERATED_BODY()

	/** Enable loading dependent levels as sub-playables. */
	UPROPERTY(config, EditAnywhere, Category = Settings)
	bool bEnableLoadSubPlayables = true;
};
