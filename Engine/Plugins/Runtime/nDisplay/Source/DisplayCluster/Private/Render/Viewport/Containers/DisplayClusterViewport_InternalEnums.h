// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Special flags for nDisplay viewport log
 */
enum class EDisplayClusterViewportShowLogMsgOnce : uint8
{
	None = 0,

	// No projection policy assigned for Viewports
	HandleStartScene_InvalidProjectionPolicy = 1 << 0,

	UpdateFrameContexts_FrameTargetRectHasZeroSize = 1 << 1,
	UpdateFrameContexts_RenderTargetRectHasZeroSize = 1 << 2,
	UpdateFrameContexts = UpdateFrameContexts_FrameTargetRectHasZeroSize | UpdateFrameContexts_RenderTargetRectHasZeroSize,

	UpdateCameraPolicy_ReferencedCameraNameIsEmpty = 1 << 3,
	UpdateCameraPolicy_ReferencedCameraNotFound = 1 << 4,
	UpdateCameraPolicy = UpdateCameraPolicy_ReferencedCameraNameIsEmpty | UpdateCameraPolicy_ReferencedCameraNotFound,

	GetViewPointCameraComponent_NoRootActorFound = 1 << 5,
	GetViewPointCameraComponent_HasAssignedViewPoint = 1 << 6,
	GetViewPointCameraComponent_NotFound = 1 << 7,
	GetViewPointCameraComponent = GetViewPointCameraComponent_NoRootActorFound | GetViewPointCameraComponent_NotFound,
};
ENUM_CLASS_FLAGS(EDisplayClusterViewportShowLogMsgOnce);
