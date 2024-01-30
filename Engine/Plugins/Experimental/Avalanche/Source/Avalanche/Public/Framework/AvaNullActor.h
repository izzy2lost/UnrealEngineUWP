// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/UnrealString.h"
#include "GameFramework/Actor.h"
#include "AvaNullActor.generated.h"

class UAvaNullComponent;

/**
 * Avalanche Null Actor (Empty Group)
 */
UCLASS(MinimalAPI, DisplayName = "Motion Design Null Actor")
class AAvaNullActor : public AActor
{
	GENERATED_BODY()

public:
	static const FString DefaultLabel;

	AAvaNullActor();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Motion Design")
	TObjectPtr<UAvaNullComponent> NullComponent;
};
