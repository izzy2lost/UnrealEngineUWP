// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Runtime configuration from DCRA.
 */
enum class EDisplayClusterViewportPreviewFlags : uint8
{
	None = 0,

	// the RTT has been changed
	HasChangedPreviewRTT = 1 << 0,

	// Preview mesh material instance has been changed.
	HasChangedPreviewMeshMaterialInstance = 1 << 1,

	// Preview editable mesh material instance has been changed.
	HasChangedPreviewEditableMeshMaterialInstance = 1 << 2,
};
ENUM_CLASS_FLAGS(EDisplayClusterViewportPreviewFlags);
