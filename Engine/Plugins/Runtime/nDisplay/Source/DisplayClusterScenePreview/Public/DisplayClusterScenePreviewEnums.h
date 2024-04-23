// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Special flags that control the behavior of the renderer.
*/
enum class EDisplayClusterScenePreviewFlags : uint8
{
	None = 0,

	/** If true, use DCRA proxy for rendering. */
	UseRootActorProxy = 1 << 0,

	/** If true, automatically update the renderer with stage actors belonging to the root actor. */
	AutoUpdateLightcards = 1 << 1,
};
ENUM_CLASS_FLAGS(EDisplayClusterScenePreviewFlags);
