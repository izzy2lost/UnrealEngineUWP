// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Render/Viewport/Containers/DisplayClusterViewport_OverscanSettings.h"

/**
 * Define the viewport type for tile rendering.
 */
enum class EDisplayClusterViewportTileType: uint8
{
	// This viewport dont use tile rendering
	None = 0,

	// This viewport will be split into several tiled viewports.
	// These tiled viewports will be used for rendering, after which the resulting images will be composited back into this viewport.
	Source,

	// One of many tiled viewports that is used to render an image fragment for the Source viewport.
	Tile,

	// This is an internal type that is used during the process of reallocation in the viewport.
	UnusedTile
};

/**
 * nDisplay viewport tile settings.
 * These are runtime settings, updated every frame from the cluster configuration.
 */
struct FDisplayClusterViewport_TileSettings
{
public:
	FDisplayClusterViewport_TileSettings() = default;

	/** Setup as source. */
	FDisplayClusterViewport_TileSettings(const FIntPoint& InSize, const FDisplayClusterViewport_OverscanSettings& InOverscanSettings)
		: Type(EDisplayClusterViewportTileType::Source), Size(InSize), OverscanSettings(InOverscanSettings)
	{ }

	/** Setup as tile. */
	FDisplayClusterViewport_TileSettings(const FString& InSourceViewportId, const FIntPoint& InPos, const FIntPoint& InSize)
		: Type(EDisplayClusterViewportTileType::Tile), Size(InSize), Pos(InPos), SourceViewportId(InSourceViewportId)
	{ }

	/** Returns the current viewport type for tile rendering. */
	inline EDisplayClusterViewportTileType GetType() const
	{
		return Type;
	}

	/** Return true if this viewport is internal. */
	inline bool IsInternalViewport() const
	{
		return Type == EDisplayClusterViewportTileType::Tile;
	}

	/** Get Size value. */
	inline const FIntPoint& GetSize() const
	{
		return Size;
	}

	/** Get Pos value. */
	inline const FIntPoint& GetPos() const
	{
		return Pos;
	}

	/** Get SourceViewportId. */
	inline const FString& GetSourceViewportId() const
	{
		return SourceViewportId;
	}

	/** Get overscan settings for tile rendering. */
	inline const FDisplayClusterViewport_OverscanSettings& GetOverscanSettings() const
	{
		return OverscanSettings;
	}

	/** Set tile state to be used. */
	inline void SetTileStateToBeUsed(bool bUsed)
	{
		if(bUsed && Type == EDisplayClusterViewportTileType::UnusedTile)
		{
			Type = EDisplayClusterViewportTileType::Tile;
		}

		if (!bUsed && Type == EDisplayClusterViewportTileType::Tile)
		{
			Type = EDisplayClusterViewportTileType::UnusedTile;
		}
	}

public:
	// Optimize overscan values for edge tiles.
	bool bOptimizeTileOverscan = false;

private:
	// Define the viewport type for tile rendering.
	EDisplayClusterViewportTileType Type = EDisplayClusterViewportTileType::None;

	// Tile size for the 'source' viewport
	FIntPoint Size = FIntPoint::ZeroValue;

	// Tile index for the 'tile' viewport
	FIntPoint Pos = FIntPoint::ZeroValue;

	// Tile source viewport name
	FString SourceViewportId;

	// Overscan settings for tile rendering.
	FDisplayClusterViewport_OverscanSettings OverscanSettings;
};
