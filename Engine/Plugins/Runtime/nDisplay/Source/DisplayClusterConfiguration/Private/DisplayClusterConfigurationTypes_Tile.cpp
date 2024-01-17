// Copyright Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterConfigurationTypes_Tile.h"
#include "DisplayClusterConfigurationTypes.h"


///////////////////////////////////////////////////////////////////////////////////////
// FDisplayClusterConfigurationViewport_Tile
///////////////////////////////////////////////////////////////////////////////////////
bool FDisplayClusterConfigurationTile_Settings::IsEnabled(const FDisplayClusterConfigurationICVFX_StageSettings& InStageSettings) const
{
	// Note: InStageSettings can be used to disable tile rendering for the entire stage.

	if (!bEnabled)
	{
		// This property should enable tile rendering.
		return false;
	}

	if (TileX < 1 || TileY < 1)
	{
		// Ignore wrong values
		return false;
	}

	if (TileX == 1 && TileY == 1)
	{
		// Ignore if only 1 tile is used.
		return false;
	}

	return true;
}
