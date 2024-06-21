// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "AvaBroadcastLibrary.generated.h"

UCLASS(MinimalAPI, DisplayName = "Motion Design Broadcast Library", meta=(ScriptName = "MotionDesignBroadcastLibrary"))
class UAvaBroadcastLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Returns the current channel's viewport size. */
	UFUNCTION(BlueprintCallable, Category = "Motion Design|broadcast", meta=(WorldContext = "InWorldContextObject"))
	static AVALANCHEMEDIA_API FVector2D GetChannelViewportSize(const UObject* InWorldContextObject);
};
