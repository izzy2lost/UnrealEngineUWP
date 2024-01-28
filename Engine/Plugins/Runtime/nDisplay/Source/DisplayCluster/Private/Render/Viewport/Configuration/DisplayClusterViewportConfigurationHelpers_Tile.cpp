// Copyright Epic Games, Inc. All Rights Reserved.

#include "Render/Viewport/Configuration/DisplayClusterViewportConfigurationHelpers_Tile.h"
#include "Render/Viewport/Configuration/DisplayClusterViewportConfigurationHelpers.h"

#include "Misc/DisplayClusterGlobals.h"
#include "Misc/DisplayClusterLog.h"

#include "Render/Viewport/DisplayClusterViewport.h"
#include "Render/Viewport/DisplayClusterViewportManager.h"
#include "Render/Viewport/DisplayClusterViewportStrings.h"
#include "Render/Viewport/DisplayClusterViewportHelpers.h"
#include "Render/Viewport/Configuration/DisplayClusterViewportConfiguration.h"

#include "DisplayClusterEnums.h"
#include "DisplayClusterConfigurationTypes_ICVFX.h"
#include "DisplayClusterConfigurationTypes_Tile.h"
#include "DisplayClusterProjectionStrings.h"

#include "IDisplayCluster.h"
#include "IDisplayClusterCallbacks.h"


////////////////////////////////////////////////////////////////////////
// FDisplayClusterViewportConfigurationHelpers_Tile
////////////////////////////////////////////////////////////////////////

void FDisplayClusterViewportConfigurationHelpers_Tile::UpdateICVFXCameraViewportTileSettings(FDisplayClusterViewport& InSourceViewport, const FDisplayClusterConfigurationICVFX_CameraTile& InCameraTile)
{
	if (const FDisplayClusterConfigurationICVFX_StageSettings* StageSettings = InSourceViewport.Configuration->GetStageSettings())
	{
		if (InCameraTile.TileSettings.IsEnabled(*StageSettings))
		{
			if (InSourceViewport.CanSplitIntoTiles())
			{
				const FDisplayClusterViewport_OverscanSettings& OverscanSettings = FDisplayClusterViewportConfigurationHelpers_Tile::GetTileOverscanSettings(InCameraTile.TileOverscan);
				const FIntPoint Size(InCameraTile.TileSettings.TileX, InCameraTile.TileSettings.TileY);

				// Set this viewport as the source for tile rendering.
				FDisplayClusterViewport_TileSettings& OutTileSettings = InSourceViewport.GetRenderSettingsImpl().TileSettings;
				OutTileSettings = FDisplayClusterViewport_TileSettings(Size, OverscanSettings);
				OutTileSettings.bOptimizeTileOverscan = InCameraTile.TileOverscan.bOptimizeTileOverscan;

				return;
			}

			// This viewport cannot be split because the current settings are conflicting.
			UE_LOG(LogDisplayClusterViewport, Error, TEXT("Viewport '%s' cannot be tiled because the current settings are conflicting."), *InSourceViewport.GetId());
		}
	}

	// By default disable tile rendering.
	InSourceViewport.GetRenderSettingsImpl().TileSettings = FDisplayClusterViewport_TileSettings();
}

FIntRect FDisplayClusterViewportConfigurationHelpers_Tile::GetDestRect(const FDisplayClusterViewport_TileSettings& InTileSettings, const FIntRect& InSourceRect)
{
	check(InTileSettings.GetType() == EDisplayClusterViewportTileType::Tile);

	const FIntPoint SourceSize(InSourceRect.Width(), InSourceRect.Height());
	const FIntPoint TileSize(FMath::RoundToZero(float(SourceSize.X) / InTileSettings.GetSize().X), FMath::RoundToZero(float(SourceSize.Y) / InTileSettings.GetSize().Y));
	const FIntPoint TilePos(InTileSettings.GetPos().X * TileSize.X, InTileSettings.GetPos().Y * TileSize.Y);

	// Dest rect min value:
	const FIntPoint DestPos = InSourceRect.Min + TilePos;

	// Dest rect size
	FIntPoint DestSize(TileSize);

	// The source size may not be divisible, and some pixels may be lost; then they will need to be restored to the edge tiles.
	if ((InTileSettings.GetPos().X + 1) == InTileSettings.GetSize().X)
	{
		DestSize.X = SourceSize.X - TilePos.X;
	}
	if ((InTileSettings.GetPos().Y + 1) == InTileSettings.GetSize().Y)
	{
		DestSize.Y = SourceSize.Y - TilePos.Y;
	}

	return FIntRect(DestPos, DestPos + DestSize);
}

FString FDisplayClusterViewportConfigurationHelpers_Tile::GetUniqueViewportNameForTile(const FString& InViewportId, const FIntPoint& TilePos)
{
	check(!InViewportId.IsEmpty());

	return FString::Printf(TEXT("%s_%s_%d_%d"), *InViewportId, DisplayClusterViewportStrings::tile::prefix, TilePos.X, TilePos.Y);
}

bool FDisplayClusterViewportConfigurationHelpers_Tile::CreateProjectionPolicyForTileViewport(FDisplayClusterViewport& InSourceViewport, const FIntPoint& TilePos, TSharedPtr<IDisplayClusterProjectionPolicy, ESPMode::ThreadSafe>& OutProjPolicy)
{
	FDisplayClusterConfigurationProjection CameraProjectionPolicyConfig;

	// Projection policy with type 'link' has support for tile rendering
	CameraProjectionPolicyConfig.Type = DisplayClusterProjectionStrings::projection::Link;

	// Create projection policy for viewport
	OutProjPolicy = FDisplayClusterViewportManager::CreateProjectionPolicy(GetUniqueViewportNameForTile(InSourceViewport.GetId(), TilePos), &CameraProjectionPolicyConfig);

	if (!OutProjPolicy.IsValid())
	{
		UE_LOG(LogDisplayClusterViewport, Error, TEXT("Tile Viewport '%s': projection policy for tile [%d-%d] not created for node '%s'."), *InSourceViewport.GetId(), TilePos.X, TilePos.Y, *InSourceViewport.GetClusterNodeId());

		return false;
	}

	return true;
}

FDisplayClusterViewport* FDisplayClusterViewportConfigurationHelpers_Tile::FindTileViewport(FDisplayClusterViewport& InSourceViewport, const FIntPoint& TilePos)
{
	if (FDisplayClusterViewportManager* ViewportManager = InSourceViewport.Configuration->GetViewportManagerImpl())
	{
		TSharedPtr<FDisplayClusterViewport, ESPMode::ThreadSafe> Viewport = ViewportManager->ImplFindViewport(GetUniqueViewportNameForTile(InSourceViewport.GetId(), TilePos));

		return Viewport.Get();
	}

	return nullptr;
}

FDisplayClusterViewport* FDisplayClusterViewportConfigurationHelpers_Tile::GetOrCreateTileViewport(FDisplayClusterViewport& InSourceViewport, const FIntPoint& InTilePos)
{
	const FDisplayClusterViewport_RenderSettings& SourceRenderSettings = InSourceViewport.GetRenderSettings();
	if (SourceRenderSettings.TileSettings.GetType() != EDisplayClusterViewportTileType::Source)
	{
		return nullptr;
	}

	const FIntPoint& InTileSize = SourceRenderSettings.TileSettings.GetSize();

	FDisplayClusterViewport* TileViewport = FindTileViewport(InSourceViewport, InTilePos);
	if (TileViewport == nullptr)
	{
		if (FDisplayClusterViewportManager* ViewportManager = InSourceViewport.Configuration->GetViewportManagerImpl())
		{
			// Create new camera viewport
			TSharedPtr<IDisplayClusterProjectionPolicy, ESPMode::ThreadSafe> TileProjectionPolicy;
			if (CreateProjectionPolicyForTileViewport(InSourceViewport, InTilePos, TileProjectionPolicy))
			{
				// Create viewport for new projection policy
				TileViewport = ViewportManager->ImplCreateViewport(GetUniqueViewportNameForTile(InSourceViewport.GetId(), InTilePos), TileProjectionPolicy).Get();
			}
		}
	}

	// Update tile viewport settings from the source viewport
	// Note: The source viewport must already be configured.
	if (TileViewport)
	{
		// Gain direct access to internal resources of the NewViewport:
		FDisplayClusterViewport_RenderSettings& InOutRenderSettings = TileViewport->GetRenderSettingsImpl();

		// Reset runtime flags from prev frame:
		TileViewport->ResetRuntimeParameters();

		// Copy all the settings from the source viewport, but some of them still need to be overridden.
		InOutRenderSettings = SourceRenderSettings;

		// Dont show Tile composing viewports on frame target
		InOutRenderSettings.bVisible = false;

		// Disable custom frustum settings and override overscan settings.
		// Use custom overscan settings for the tile rendering.
		InOutRenderSettings.CustomFrustumSettings = FDisplayClusterViewport_CustomFrustumSettings();
		InOutRenderSettings.OverscanSettings = SourceRenderSettings.TileSettings.GetOverscanSettings();

		// Optimize overscan values for edge tiles
		if (SourceRenderSettings.TileSettings.bOptimizeTileOverscan)
		{
			if (InTilePos.X == 0)
			{
				InOutRenderSettings.OverscanSettings.Left = 0;
			}
			if (InTilePos.Y == 0)
			{
				InOutRenderSettings.OverscanSettings.Top = 0;
			}
			if (InTilePos.X == (InTileSize.X - 1))
			{
				InOutRenderSettings.OverscanSettings.Right = 0;
			}
			if (InTilePos.Y == (InTileSize.Y - 1))
			{
				InOutRenderSettings.OverscanSettings.Bottom = 0;
			}
		}

		// Setup as tile.
		InOutRenderSettings.TileSettings = FDisplayClusterViewport_TileSettings(InSourceViewport.GetId(), InTilePos, InTileSize);

		// Allow external customers to configure media state
		{
			// By default, we set 'None' so the tiles can be rendered in editor for camera preview.
			EDisplayClusterViewportMediaState NewMediaStates = EDisplayClusterViewportMediaState::None;

			// But in cluster mode, we set 'Inactive' by default. When tiling is used, the tile viewport must either be rendered for media output
			// or preserve internal buffer for a media input texture. However, it's possible the tile viewport has wrong or completely
			// missing media output settings, or corresponding media device was not able to start properly for some reason.
			// In this case the tile should not be rendered at all.
			if (GDisplayCluster->GetOperationMode() == EDisplayClusterOperationMode::Cluster)
			{
				NewMediaStates = EDisplayClusterViewportMediaState::Inactive;
			}
			
			// Now allow to override media state if anyone wants
			IDisplayCluster::Get().GetCallbacks().OnDisplayClusterUpdateViewportMediaState().Broadcast(TileViewport, NewMediaStates);

			// Update the media state for the new frame.
			InOutRenderSettings.AssignMediaStates(NewMediaStates);
		}

		// Copy internal render settings from the source:
		TileViewport->GetVisibilitySettingsImpl() = InSourceViewport.GetVisibilitySettingsImpl();
		TileViewport->GetCameraMotionBlurImpl() = InSourceViewport.GetCameraMotionBlurImpl();
		TileViewport->GetCameraDepthOfFieldImpl() = InSourceViewport.GetCameraDepthOfFieldImpl();

		// Copy OCIO.
		TileViewport->SetOpenColorIO(InSourceViewport.GetOpenColorIO());
	}

	return TileViewport;
}

FDisplayClusterViewport_OverscanSettings FDisplayClusterViewportConfigurationHelpers_Tile::GetTileOverscanSettings(const FDisplayClusterConfigurationTile_Overscan& InTileOverscan)
{
	FDisplayClusterViewport_OverscanSettings OutOverscanSettings;

	OutOverscanSettings.bEnabled = false;
	OutOverscanSettings.bOversize = InTileOverscan.bOversize;

	if (InTileOverscan.bEnabled)
	{
		switch (InTileOverscan.Mode)
		{
		case EDisplayClusterConfigurationViewportOverscanMode::Percent:
			OutOverscanSettings.bEnabled = InTileOverscan.bEnabled;
			OutOverscanSettings.Unit = EDisplayClusterViewport_FrustumUnit::Percent;

			// Scale 0..100% to 0..1 range
			OutOverscanSettings.Left =
			OutOverscanSettings.Right = 
			OutOverscanSettings.Top = 
			OutOverscanSettings.Bottom = .01f * InTileOverscan.AllSides;
			break;

		case EDisplayClusterConfigurationViewportOverscanMode::Pixels:
			OutOverscanSettings.bEnabled = InTileOverscan.bEnabled;
			OutOverscanSettings.Unit = EDisplayClusterViewport_FrustumUnit::Pixels;

			OutOverscanSettings.Left =
			OutOverscanSettings.Right =
			OutOverscanSettings.Top =
			OutOverscanSettings.Bottom = InTileOverscan.AllSides;
			break;

		default:
			break;
		}
	}

	return OutOverscanSettings;
}
