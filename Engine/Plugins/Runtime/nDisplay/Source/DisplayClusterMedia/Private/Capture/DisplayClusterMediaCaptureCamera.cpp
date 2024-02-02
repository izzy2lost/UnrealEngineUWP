// Copyright Epic Games, Inc. All Rights Reserved.

#include "Capture/DisplayClusterMediaCaptureCamera.h"

#include "Components/DisplayClusterICVFXCameraComponent.h"
#include "Config/IDisplayClusterConfigManager.h"
#include "DisplayClusterConfigurationTypes_Viewport.h"
#include "DisplayClusterRootActor.h"
#include "IDisplayCluster.h"
#include "IDisplayClusterCallbacks.h"
#include "Game/IDisplayClusterGameManager.h"

#include "Render/Viewport/IDisplayClusterViewportManagerProxy.h"
#include "RHICommandList.h"
#include "RHIResources.h"


FDisplayClusterMediaCaptureCamera::FDisplayClusterMediaCaptureCamera(const FString& InMediaId, const FString& InClusterNodeId, const FString& InCameraId, const FString& InViewportId, UMediaOutput* InMediaOutput, UDisplayClusterMediaOutputSynchronizationPolicy* SyncPolicy)
	: FDisplayClusterMediaCaptureViewport(InMediaId, InClusterNodeId, InViewportId, InMediaOutput, SyncPolicy)
	, CameraId(InCameraId)
{
}
