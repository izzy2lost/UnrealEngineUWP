// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "UObject/ObjectMacros.h"
#include "AvaObjectData.generated.h"

USTRUCT()
struct FAvaObjectData
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<uint8> SerializedData;

	UPROPERTY()
	uint64 ObjectFlags{};

	EObjectFlags GetObjectFlags() const { return static_cast<EObjectFlags>(ObjectFlags); }
};
