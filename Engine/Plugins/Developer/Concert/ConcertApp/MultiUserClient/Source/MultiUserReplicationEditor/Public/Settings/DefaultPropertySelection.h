// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DefaultPropertySelection.generated.h"

USTRUCT()
struct FDefaultPropertySelection
{
	GENERATED_BODY()

	/**
	 * A list of properties that should be selected by default when you add a new object type.
	 * Specify properties by using the names as the appear in the replication editor and each property with ".".
	 * Example: "RelativeLocation.X"
	 */
	UPROPERTY(EditAnywhere, Category = "Config")
	TArray<FString> DefaultSelectedProperties;

	/** Whether to inherit the properties from the base class (you need not list them in DefaultSelectedProperties). */
	UPROPERTY(EditAnywhere, Category = "Config")
	bool bInheritFromBase = true;
};
