// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/DeveloperSettingsBackedByCVars.h"
#include "VirtualScoutingSettings.generated.h"


/**
 * Per project settings for Virtual Scouting.
 */
UCLASS(Config=VirtualScoutingSettings, DefaultConfig, DisplayName="Virtual Scouting")
class VIRTUALSCOUTING_API UVirtualScoutingSettings : public UObject
{
	GENERATED_BODY()
	
public:

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category="Virtual Scouting", meta=(DisplayName="Show Measurements in Imperial Units"))
	bool bUseImperial = false;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category="Virtual Scouting", meta=(DisplayName="Viewfinder Apertures"))
	TArray<float> ViewfinderApertureArray = {1.0,1.4,2.0,2.8,4.0,5.6,8.0,11.0,16.0,22.0};

	UFUNCTION(BlueprintPure, Category="Virtual Scouting", DisplayName="Virtual Scouting Settings")
	static UVirtualScoutingSettings* GetVirtualScoutingSettings();
};

/**
 * Per user settings for Virtual Scouting Editor.
 */
UCLASS(Config=EditorPerProjectUserSettings, DisplayName="Virtual Scouting Editor Settings")
class VIRTUALSCOUTING_API UVirtualScoutingEditorSettings : public UObject
{
	GENERATED_BODY()
	
public:

	UFUNCTION(BlueprintPure, Category="Virtual Scouting Editor")
	static UVirtualScoutingEditorSettings* GetVirtualScoutingEditorSettings();
};
