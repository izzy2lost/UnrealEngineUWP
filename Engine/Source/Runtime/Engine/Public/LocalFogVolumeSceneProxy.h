// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Math/Color.h"

class ULocalFogVolumeComponent;

/** Represents a UVolumetricCloudComponent to the rendering thread, created game side from the component. */
class FLocalFogVolumeSceneProxy
{
public:

	// Initialization constructor.
	ENGINE_API FLocalFogVolumeSceneProxy(const ULocalFogVolumeComponent* InComponent);
	ENGINE_API ~FLocalFogVolumeSceneProxy();

	FTransform FogTransform;

	float FogDensity;
	float FogHeightFalloff;
	float FogHeightOffset;
	float FogRadialAttenuation;

	uint8 FogMode;
	uint8 FogSortPriority;

	float FogPhaseG;
	FLinearColor FogAlbedo;
	FLinearColor FogEmissive;
private:
};
