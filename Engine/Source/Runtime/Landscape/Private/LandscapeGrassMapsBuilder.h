// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "LandscapeComponent.h"

#if WITH_EDITOR

/**
 * Helper class used to Build or monitor outdated Grass maps of a world
 */
class FLandscapeGrassMapsBuilder
{
public:
	LANDSCAPE_API FLandscapeGrassMapsBuilder(UWorld* InWorld);
	LANDSCAPE_API void Build();
	LANDSCAPE_API int32 GetOutdatedGrassMapCount(bool bInForceUpdate = true) const;
private:
	UWorld* World;
	mutable int32 OutdatedGrassMapCount;
	mutable double GrassMapsLastCheckTime;
};

#endif // WITH_EDITOR
