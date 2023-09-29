// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "OpenXRInputSettings.generated.h"

/**
* Implements the settings for the OpenXR Input plugin.
*/
UCLASS(config = Input, defaultconfig)
class OPENXRINPUT_API UOpenXRInputSettings : public UObject
{
	GENERATED_UCLASS_BODY()

public:
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	/** Set a mappable input config to allow OpenXR runtimes to remap the Enhanced Input actions. */
	UPROPERTY(config)
	FSoftObjectPath MappableInputConfig = nullptr;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	/** Set a mappable input config to allow OpenXR runtimes to remap the Enhanced Input actions. */
	UPROPERTY(config, EditAnywhere, Category = "Enhanced Input", meta = (DisplayName = "Input Mapping Contexts for XR"))
	TSet<TSoftObjectPtr<class UInputMappingContext>> InputMappingContexts;

	// UObject interface
#if WITH_EDITOR
	virtual void PostInitProperties() override;
#endif
};
