// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ComponentInstanceDataCache.h"
#include "AvalancheComponentData.generated.h"

USTRUCT()
struct FAvalancheComponentData
{
	GENERATED_BODY()

	/** Describes how the component was created */
	UPROPERTY()
	EComponentCreationMethod CreationMethod{};
};
