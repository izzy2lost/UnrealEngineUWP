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

	/**
	 * Injects the remote control values from current transition for the current playable.
	 * @remark This does nothing if there is no current transition the current playable is part of or if
	 *		the current level is not managed by a playable.
	 * @return true if the values have been injected, false otherwise.
	 */
	UFUNCTION(BlueprintCallable, Category = "Motion Design|Playable", meta=(WorldContext = "InWorldContextObject"))
	static AVALANCHEMEDIA_API bool UpdateRemoteControlValues(const UObject* InWorldContextObject);
};
