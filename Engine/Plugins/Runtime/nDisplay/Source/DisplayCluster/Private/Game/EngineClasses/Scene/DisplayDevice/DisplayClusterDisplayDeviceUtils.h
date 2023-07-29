// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

class UDisplayClusterConfigurationViewport;
class UDisplayClusterDisplayDeviceBaseComponent;

namespace UE::DisplayClusterDisplayDeviceUtils
{
#if WITH_EDITOR
	/**
	 * Attempt to find the display device given a viewport.
	 * If the display device is not found on an instance, the CDO is checked in case the display device was renamed and the
	 * viewport on the instance will be synced to the new name.
	 *
	 * @param InViewport The viewport object which points to a display device and belongs to a root actor.
	 * @return The display device this viewport uses or nullptr.
	 */
	UDisplayClusterDisplayDeviceBaseComponent* FindAndSyncDisplayDeviceFromViewport(UDisplayClusterConfigurationViewport* InViewport);
#endif
}
