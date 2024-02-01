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
	 * Creates World Assets based on the Motion Design Blueprints provided
	 * Wraps FAvaEditorFunctionLibrary::ExportMotionDesignBlueprintsToWorld
	 * @param InBlueprints the Motion Design Blueprints to export
	 */
	UFUNCTION(BlueprintCallable, Category = "Motion Design")
	static void ExportMotionDesignBlueprintsToWorld(const TArray<TSoftObjectPtr<UAvalancheBlueprint>>& InBlueprints);
};
