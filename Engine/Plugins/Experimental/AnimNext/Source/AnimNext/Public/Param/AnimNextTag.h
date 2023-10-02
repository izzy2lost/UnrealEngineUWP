// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AnimNextTag.generated.h"

// Empty struct to act as a named tag for parameters
USTRUCT()
struct FAnimNextTag
{
	GENERATED_BODY()
};

// Empty struct to tag named scopes
USTRUCT()
struct FAnimNextScope : public FAnimNextTag
{
	GENERATED_BODY()
};
