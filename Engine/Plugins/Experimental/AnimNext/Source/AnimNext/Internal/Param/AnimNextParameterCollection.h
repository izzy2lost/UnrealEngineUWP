// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Param/AnimNextParameterSourceRef.h"
#include "AnimNextParameterCollection.generated.h"

class UAnimNextParameterBlock;

// Struct wrapper to allow an array of parameters to be nested in a container or edited & referenced in bulk
USTRUCT()
struct FAnimNextParameterCollection
{
	GENERATED_BODY()

	// User-editable parameter blocks
	UPROPERTY(EditAnywhere, Category = "Parameters")
	TArray<FAnimNextParameterSourceRef> Parameters;
};