// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"


/**
 * Media customization utilities
 */
class FDisplayClusterConfiguratorMediaUtils
{
public:

	/** Singleton access */
	static FDisplayClusterConfiguratorMediaUtils& Get();

public:

	/** Tiled media auto-configuration event */
	DECLARE_EVENT_OneParam(FDisplayClusterConfiguratorMediaUtils, FDisplayClusterConfiguratorTiledMediaAutoConfigurationRequestedEvent, UObject*);
	FDisplayClusterConfiguratorTiledMediaAutoConfigurationRequestedEvent& OnTiledMediaAutoConfiguration()
	{
		return TiledMediaAutoConfigurationEvent;
	}

private:

	/** Tiled media auto-configuration event */
	FDisplayClusterConfiguratorTiledMediaAutoConfigurationRequestedEvent TiledMediaAutoConfigurationEvent;
};
