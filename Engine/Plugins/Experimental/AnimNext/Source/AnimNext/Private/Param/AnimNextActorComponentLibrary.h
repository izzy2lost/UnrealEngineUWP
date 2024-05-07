// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Scheduler/AnimNextTickFunctionBinding.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "AnimNextActorComponentLibrary.generated.h"

class UActorComponent;

// Access to non-UProperty/UFunction data on UActorComponent
UCLASS()
class UAnimNextActorComponentLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// Returns the component's tick function
	UFUNCTION(BlueprintCallable, BlueprintInternalUseOnly)
	static FAnimNextTickFunctionBinding GetTick(UActorComponent* InComponent);
};
