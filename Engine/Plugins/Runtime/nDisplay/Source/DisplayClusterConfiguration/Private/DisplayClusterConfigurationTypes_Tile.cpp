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
		// This property enables tile rendering.
		return false;
	}

	return FDisplayClusterConfigurationTile_Settings::IsEnabled(FIntPoint{TileX, TileY}, InStageSettings);
}

bool FDisplayClusterConfigurationTile_Settings::IsEnabled(const FIntPoint& InTileLoc, const struct FDisplayClusterConfigurationICVFX_StageSettings& InStageSettings)
{
	// Ignore wrong values
	if (InTileLoc.X < 1 || InTileLoc.Y < 1)
	{
		return false;
	}

	// Ignore 1x1 case
	if (InTileLoc.X == 1 && InTileLoc.Y == 1)
	{
		return false;
	}

	return true;
}
