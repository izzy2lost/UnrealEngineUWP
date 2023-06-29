// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"

#include "DisplayClusterWarpBlueprint_Enums.generated.h"

/**
 * Projection mode for the camera that is used as an image source
 * This projection does not directly use slices from the camera image,
 * but calculates the camera sub-frustum used to render the sub-images of camera for a particular viewport.
 * Since the aspect ratio of the AABB geometry in the viewport cannot be equal
 * to the aspect ratio of the camera, we must use several methods to fit the camera to the geometry.
 */
UENUM()
enum class EDisplayClusterWarpCameraProjectionMode : uint8
{
	/** Use camera as source of image.Preserving the camera aspect ratio.
	 * The camera frame touches the geometry inside
	 */
	TouchInside UMETA(DisplayName = "Touch Inside"),

	/** Use camera as source of image.Preserving the camera aspect ratio.
	 * The camera frame touches the geometry outside
	 */
	TouchOutside  UMETA(DisplayName = "Touch Outside"),

	/** Use camera as source of image.
	 * Stretch the camera image to geometry (the camera aspect ratio will be lost)
	 */
	Stretch  UMETA(DisplayName = "Stretch"),
};
