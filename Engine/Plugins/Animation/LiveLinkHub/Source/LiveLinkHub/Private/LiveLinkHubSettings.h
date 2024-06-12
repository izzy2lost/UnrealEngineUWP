// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"

#include "LiveLinkHubSettings.generated.h"

/**
 * Settings for LiveLinkHub.
 */
UCLASS(config=Engine, defaultconfig)
class LIVELINKHUB_API ULiveLinkHubSettings : public UObject
{
	GENERATED_BODY()

public:
	/** If enabled, discovered clients will be automatically added to the current session. */
	UPROPERTY(config, EditAnywhere, Category="LiveLinkHub", DisplayName = "Automatically add discovered clients")
	bool bAutoAddDiscoveredClients = true;

	/** The size in megabytes to buffer when streaming a recoding. */
	UPROPERTY(config, EditAnywhere, Category="LiveLinkHub", meta = (ClampMin = "1", UIMin = "1"))
	int32 FrameBufferSizeMB = 100;
	
	/** Which project settings sections to display when opening the settings viewer. */
	UPROPERTY(config)
	TArray<FName> ProjectSettingsToDisplay;
};
