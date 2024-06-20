// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "AvaPlayableLibrary.generated.h"

class UAvaPlayable;
class UAvaPlayableTransition;

UCLASS(MinimalAPI, DisplayName = "Motion Design Playable Library", meta=(ScriptName = "MotionDesignPlayableLibrary"))
class UAvaPlayableLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Returns the playable corresponding to that world object. */
	static UAvaPlayable* GetPlayable(const UObject* InWorldContextObject);

	/** Returns the transition this playable is part of. */
	static UAvaPlayableTransition* GetPlayableTransition(const UAvaPlayable* InPlayable);

	/** Injects the remote control values from current transition for the current playable. */
	UFUNCTION(BlueprintCallable, Category = "Motion Design|Playable", meta=(WorldContext = "InWorldContextObject"))
	static AVALANCHEMEDIA_API void UpdateRemoteControlValues(const UObject* InWorldContextObject);
};
