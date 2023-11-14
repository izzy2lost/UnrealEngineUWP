// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "OXRVisionOSSettingsTypes.h"

#include "OXRVisionOSRuntimeSettings.generated.h"

UCLASS(config=Engine, defaultconfig, meta=(DisplayName="OXRVisionOS"))
class OXRVISIONOSSETTINGS_API UOXRVisionOSRuntimeSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	// Defines a rotation adjustment to the aim pose from the OXRVisionOS native controller orientation.  This pose may be fine without adjustment.
	UPROPERTY(config, EditAnywhere, Category=OXRVisionOS, Meta = (DisplayName = "Aim Pose Adjustment"))
	FRotator OXRVisionOSAimPoseAdjustment = FRotator(0.0f, 0.0f, 0.0f);

	// Defines a rotation adjustment to the grip pose from the OXRVisionOS native controller orientation.  We provide a default adjustment to grip.
	UPROPERTY(config, EditAnywhere, Category = OXRVisionOS, Meta = (DisplayName = "Grip Pose Adjustment"))
	FRotator OXRVisionOSGripPoseAdjustment = FRotator(45.0f,0.0f,0.0f);

	// Number of UI2D/quad layers supported by OXRVisionOS, required at app startup time
	UPROPERTY(config, EditAnywhere, Category = OXRVisionOS, Meta = (DeprecatedProperty, ClampMin = "0", ClampMax = "4", DisplayName = "Max Number UI Layers"))
	int32 OXRVisionOSNumUILayers = 0;
};
