// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AndroidSingleInstanceServiceRuntimeSettings.generated.h"

/**
* Implements the settings for the AndroidSingleInstanceService plugin.
*/


UCLASS(Config = Engine, DefaultConfig)
class UAndroidSingleInstanceServiceRuntimeSettings : public UObject
{
	GENERATED_UCLASS_BODY()

	// Enable Android SingleInstanceService for packaged builds and quick launch
	UPROPERTY(EditAnywhere, config, Category = Packaging, Meta = (DisplayName = "Use AndroidSingleInstanceService"))
	bool bEnablePlugin;

	// Compile standalone ASIS project
	UPROPERTY(EditAnywhere, config, Category = Packaging, Meta=(EditCondition = "bEnablePlugin"))
	bool bCompileASISProject;

private:

};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#endif
