// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGCommon.h"
#include "UObject/Object.h"

#include "PCGBuilderSettings.generated.h"

class UPCGGraphInterface;

UCLASS(hidecategories = Object)
class UPCGBuilderSettings : public UObject
{
	GENERATED_BODY()

public:
	UPCGBuilderSettings();

	/** Graphs that will be generated in the specified order. When left empty, all Graphs will get generated in the order they are discovered. */
	UPROPERTY(EditAnywhere, Category = Build)
	TArray<TSoftObjectPtr<UPCGGraphInterface>> Graphs;

	/** Include components which have the following editing mode. */
	UPROPERTY(EditAnywhere, Category = Build)
	TArray<EPCGEditorDirtyMode> EditingModes;

	/** Filter generated components by owner actor name. */
	UPROPERTY(EditAnywhere, Category = Filter)
	TArray<FString>  FilterByActorNames;

	/** Call generate on each component and wait until completion and any async processes before generating the next. */
	UPROPERTY(EditAnywhere, Category = Build, meta = (DisplayName = "One Component at a Time"))
	bool bOneComponentAtATime = false;

	/** Submit dirty files even if errors occurred during generation. */
	UPROPERTY(EditAnywhere, Category = Advanced)
	bool bIgnoreGenerationErrors = false;
};