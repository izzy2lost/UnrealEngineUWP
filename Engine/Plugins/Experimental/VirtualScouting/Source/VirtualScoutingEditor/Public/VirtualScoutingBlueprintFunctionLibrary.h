// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "VirtualScoutingBlueprintFunctionLibrary.generated.h"

/**
 * 
 */
UCLASS()
class VIRTUALSCOUTINGEDITOR_API UVirtualScoutingBlueprintFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

	UFUNCTION(BlueprintCallable, Category="VirtualScouting")
	static bool CheckIsWithEditor();

	/** Helper function to delete actors. This skips reference checks.*/
	UFUNCTION(BlueprintCallable, Category="VirtualScouting")
	static bool DeleteActors(const TArray<AActor*>& InActorsToDelete);

};
