// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ConcertObjectReplicationSettings.generated.h"

/** Describes how a specific object is supposed to be replicated. */
USTRUCT()
struct FConcertObjectReplicationSettings
{
	GENERATED_BODY()

	/** How often per second this object should be updated. */
	UPROPERTY(EditAnywhere, Category = "Concert")
	float UpdateFrequency = 30.f;
};

