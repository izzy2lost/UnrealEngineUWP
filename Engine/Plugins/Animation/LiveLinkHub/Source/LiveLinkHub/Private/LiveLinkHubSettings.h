// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"

#include "Misc/FrameRate.h"

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

	/**
	 * - Experimental - If this is disabled, LiveLinkHub's LiveLink Client will tick outside of the game thread.
	 * This allows processing LiveLink frame snapshots without the risk of being blocked by the game / ui thread.
	 * Note that this should only be relevant for virtual subjects since data is already forwarded to UE outside of the game thread.
	 */
	UPROPERTY(config, EditAnywhere, Category = "LiveLinkHub", meta = (ConfigRestartRequired = true))
	bool bTickOnGameThread = false;

	/** Target framerate for ticking LiveLinkHub. */
	UPROPERTY(config, EditAnywhere, Category="LiveLinkHub", meta = (ConfigRestartRequired = true, ClampMin="15.0"))
	float TargetFrameRate = 60.0f;
};
