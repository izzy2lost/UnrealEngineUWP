// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AnimNextParamInstanceIdentifier.generated.h"

// TODO: Move this to UncookedOnly module once schedules are refactored - this should be an editor-only type
// Base struct that abstracts an identifier for parameter source instances
USTRUCT(meta=(Hidden))
struct FAnimNextParamInstanceIdentifier
{
	GENERATED_BODY()

	virtual ~FAnimNextParamInstanceIdentifier() = default;

	// Check the identifier for validity
	virtual bool IsValid() const { return false; }

	// Get an FName corresponding to this instance identifier 
	virtual FName ToName() const { return NAME_None; }

	// Set up this identifier from the supplied name
	virtual void FromName(FName InName) {}
};
