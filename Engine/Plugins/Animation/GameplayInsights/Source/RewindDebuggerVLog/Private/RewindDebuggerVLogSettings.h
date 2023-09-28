// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Logging/LogVerbosity.h"
#include "RewindDebuggerVLogSettings.generated.h"

/**
 * Settings for the Rewind Debugger Visual Logger integration.
 */
UCLASS(Config=Editor, meta=(DisplayName="Rewind Debugger - Visual Logging"))
class URewindDebuggerVLogSettings : public UDeveloperSettings
{
	GENERATED_BODY()
	
public:
	URewindDebuggerVLogSettings();

#if WITH_EDITOR
	virtual FText GetSectionText() const override;
	virtual FText GetSectionDescription() const override;
#endif

	virtual FName GetCategoryName() const override;

	// Display Visual Logger shapes above this verbosity level
	UPROPERTY(EditAnywhere, Config, Category = VisualLogger)
	uint8 DisplayVerbosity = ELogVerbosity::Display;

	// Display Visual Logger shapes in these categories
	UPROPERTY(EditAnywhere, Config, Category = VisualLogger)
	TSet<FName> DisplayCategories;

	static URewindDebuggerVLogSettings & Get();
};
