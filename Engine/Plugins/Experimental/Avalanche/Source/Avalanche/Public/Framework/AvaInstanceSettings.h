// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaInstanceSettings.generated.h"

/**
 * Settings applied when instancing a Motion Design blueprint for playback.
 */
USTRUCT()
struct FAvaInstanceSettings
{
	GENERATED_BODY()

	/** Enable loading dependent levels as sub-playables. */
	UPROPERTY(config, EditAnywhere, Category = Settings)
	bool bEnableLoadSubPlayables = true;
};
