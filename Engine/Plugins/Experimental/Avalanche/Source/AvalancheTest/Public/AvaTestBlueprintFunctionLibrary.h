// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"

#include "AvaTestBlueprintFunctionLibrary.generated.h"

class UAvalancheBlueprint;

/**
 * 
 */
UCLASS()
class AVALANCHETEST_API UAvaTestBlueprintFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Creates World Assets based on the Avalanche Blueprints provided
	 * Wraps FAvaEditorFunctionLibrary::ExportAvaBlueprintsToWorld
	 * @param InBlueprints the Avalanche Blueprints to export
	 */
	UFUNCTION(BlueprintCallable, Category = "Motion Design")
	static void ExportAvaBlueprintsToWorld(const TArray<TSoftObjectPtr<UAvalancheBlueprint>>& InBlueprints);
};
