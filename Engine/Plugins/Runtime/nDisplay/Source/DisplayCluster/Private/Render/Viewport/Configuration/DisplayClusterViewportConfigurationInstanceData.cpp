// Copyright Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterViewportConfigurationInstanceData.h"

#include "Render/Projection/IDisplayClusterProjectionPolicy.h"

#include "Render/Viewport/DisplayClusterViewport.h"
#include "Render/Viewport/DisplayClusterViewportManager.h"
#include "Render/Viewport/RenderFrame/DisplayClusterRenderFrameSettings.h"

#include "DisplayClusterRootActor.h"

#include "Render/Viewport/Configuration/DisplayClusterViewportConfigurationHelpers.h"

void FDisplayClusterViewportConfigurationInstanceData::CreateOrUpdateViewportInstance(ADisplayClusterRootActor& RootActor, FDisplayClusterViewportManager& ViewportManager, FDisplayClusterRenderFrameSettings& RenderFrameSettings)
{
	// Store current cluster node
	const FString CurrentClusterNodeID = RenderFrameSettings.ClusterNodeId;

	// This function can create viewports from other nodes in the cluster
	RenderFrameSettings.ClusterNodeId = ClusterNodeId;

	const TSharedPtr<FDisplayClusterViewport, ESPMode::ThreadSafe> ExistViewport = ViewportManager.ImplFindViewport(ViewportId);
	if (ExistViewport.IsValid())
	{
		UpdateViewportConfiguration(*ExistViewport, ViewportManager, RootActor, Configuration);

		// Store viewpoint of exist viewport
		ViewPointCameraComponent = ExistViewport->GetViewPointCameraComponent();

		// Store Viewport
		Viewport = ExistViewport;
	}
	else
	{
		if (FDisplayClusterViewport* NewViewport = ViewportManager.CreateViewport(ViewportId, Configuration))
		{
			// Store viewpoint of new viewport
			ViewPointCameraComponent = NewViewport->GetViewPointCameraComponent();

			// Store Viewport
			Viewport = NewViewport->AsShared();
		}
	}

	// Restore current cluster node id
	RenderFrameSettings.ClusterNodeId = CurrentClusterNodeID;
}

bool FDisplayClusterViewportConfigurationInstanceData::UpdateViewportConfiguration(FDisplayClusterViewport& DstViewport, FDisplayClusterViewportManager& ViewportManager, ADisplayClusterRootActor& RootActor, const UDisplayClusterConfigurationViewport& ConfigurationViewport)
{
	check(IsInGameThread());

	FDisplayClusterViewportConfigurationHelpers::UpdateBaseViewportSetting(DstViewport, RootActor, ConfigurationViewport);
	FDisplayClusterViewportConfigurationHelpers::UpdateProjectionPolicy(DstViewport, &(ConfigurationViewport.ProjectionPolicy));

	return true;
}

void FDisplayClusterViewportConfigurationInstanceData::SetViewportWarpPolicy(const TSharedPtr<FDisplayClusterViewport, ESPMode::ThreadSafe>& InViewport, IDisplayClusterWarpPolicy* InWarpPolicy)
{
	// ignore internal resources
	if (InViewport.IsValid() && !EnumHasAnyFlags(InViewport->GetRenderSettingsICVFX().RuntimeFlags, EDisplayClusterViewportRuntimeICVFXFlags::InternalResource))
	{
		if (InViewport->GetProjectionPolicy().IsValid())
		{
			InViewport->GetProjectionPolicy()->SetWarpPolicy(InWarpPolicy);
		}
	}
}
