// Copyright Epic Games, Inc. All Rights Reserved.

#include "Render/Viewport/Containers/DisplayClusterViewport_CameraDepthOfField.h"

#include "SceneView.h"

void FDisplayClusterViewport_CameraDepthOfField::SetupSceneView(FSceneView& InOutView) const
{
	InOutView.bEnableDynamicCocOffset = bEnableDepthOfFieldCompensation;
	InOutView.InFocusDistance = DistanceToWall + DistanceToWallOffset;
}