// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ConcertInheritableClassOption.h"
#include "ConcertDefaultPropertySelection.generated.h"

USTRUCT()
struct FConcertDefaultPropertySelection : public FConcertInheritableClassOption
{
	GENERATED_BODY()

	/**
	 * A list of properties that should be selected by default when you add a new object type.
	 * Specify properties by using the names as the appear in the replication editor and each property with ".".
	 * Example: "RelativeLocation.X"
	 */
	UPROPERTY(EditAnywhere, Category = "Config")
	TArray<FString> DefaultSelectedProperties;
};
