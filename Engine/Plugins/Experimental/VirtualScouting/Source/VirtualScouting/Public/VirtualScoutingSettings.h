// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/DeveloperSettingsBackedByCVars.h"
#include "VirtualScoutingSettings.generated.h"


/**
 * Virtual Scouting Settings 
 */
UCLASS(Config=VirtualScoutingSettings, DisplayName="Virtual Scouting")
class VIRTUALSCOUTING_API UVirtualScoutingSettings : public UDeveloperSettingsBackedByCVars
{
	GENERATED_BODY()
	
public:
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category="Virtual Scouting", meta=(DisplayName="Show Measurements in Imperial Units"))
	bool bUseImperial = false;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category="Virtual Scouting", meta=(DisplayName="Camera Apertures"))
	TArray<float> CameraApertureArray = {1.0,1.4,2.0,2.8,4.0,5.6,8.0,11.0,16.0,22.0};

	UFUNCTION(BlueprintPure, Category="Virtual Scouting")
	static UVirtualScoutingSettings* GetVirtualScoutingSettings();
};
