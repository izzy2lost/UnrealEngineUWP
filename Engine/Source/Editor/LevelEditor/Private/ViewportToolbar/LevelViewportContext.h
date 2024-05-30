// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"
#include "SLevelViewport.h"
#include "UObject/Object.h"

#include "LevelViewportContext.generated.h"

UCLASS()
class LEVELEDITOR_API ULevelViewportContext : public UObject
{
	GENERATED_BODY()

public:
	TWeakPtr<SLevelViewport> LevelViewport;
	// This allows the viewport toolbar to know if it needs to refresh itself to update the top-level state of the
	// realtime button.
	TOptional<bool> CachedShouldShowRealtimeOffWarning;
};
