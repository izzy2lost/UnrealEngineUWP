// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ComponentInstanceDataCache.h"
#include "AvaComponentData.generated.h"

USTRUCT()
struct FAvaComponentData
{
	GENERATED_BODY()

	/** Describes how the component was created */
	UPROPERTY()
	EComponentCreationMethod CreationMethod{};
};
