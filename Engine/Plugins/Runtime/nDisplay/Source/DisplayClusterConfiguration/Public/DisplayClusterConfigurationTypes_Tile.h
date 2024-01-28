// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "DisplayClusterConfigurationTypes_Base.h"
#include "DisplayClusterConfigurationTypes_Enums.h"

#include "DisplayClusterConfigurationTypes_Tile.generated.h"

/*
 * Tile rendering
 */
USTRUCT(Blueprintable)
struct DISPLAYCLUSTERCONFIGURATION_API FDisplayClusterConfigurationTile_Settings
{
	GENERATED_BODY()

public:
	/** Return true if tile rendering can be used. */
	bool IsEnabled(const struct FDisplayClusterConfigurationICVFX_StageSettings& InStageSettings) const;

public:
	/** Enable tile rendering. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tile Rendering")
	bool bEnabled = false;

	/** The viewport will be horizontally partitioned into the specified number of tiles.  */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tile Rendering", meta = (DisplayName = "Tile X", ClampMin = "1", UIMin = "1", ClampMax = "4", UIMax = "4"))
	int32 TileX = 1;

	/** The viewport will be vertically partitioned into the specified number of tiles.  */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tile Rendering", meta = (DisplayName = "Tile Y", ClampMin = "1", UIMin = "1", ClampMax = "4", UIMax = "4"))
	int32 TileY = 1;
};

USTRUCT(Blueprintable)
struct DISPLAYCLUSTERCONFIGURATION_API FDisplayClusterConfigurationTile_Overscan
{
	GENERATED_BODY()

public:
	/** Enable/disable Viewport Overscan and specify units as percent or pixel values. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tile Overscan", meta = (DisplayName = "Enable"))
	bool bEnabled = false;

	/** Set to True to render at the overscan resolution, set to false to render at the resolution in the configuration and scale for overscan. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tile Overscan", meta = (DisplayName = "Adapt Resolution", DisplayAfter = "Mode"))
	bool bOversize = true;

	/** Optimize overscan values on boundary tiles.
	* When enabled, tile sides not in contact with other tiles will use zero overscan. */
	UPROPERTY()
	bool bOptimizeTileOverscan = true;

	/** Enable/disable Viewport Overscan and specify units as percent or pixel values. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tile Overscan")
	EDisplayClusterConfigurationViewportOverscanMode Mode = EDisplayClusterConfigurationViewportOverscanMode::Percent;

	/** Overscan value for all sides. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tile Overscan")
	float AllSides = 10;
};
