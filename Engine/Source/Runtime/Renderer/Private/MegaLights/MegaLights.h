// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FSceneViewFamily;
struct FGlobalShaderPermutationParameters;

namespace ECastRayTracedShadow
{
	enum Type : int;
};

// Public MegaLights interface
namespace MegaLights
{
	bool IsEnabled();

	bool IsUsingClosestHZB();
	bool IsUsingGlobalSDF(const FSceneViewFamily& ViewFamily);
	bool IsUsingLightFunctions();

	bool IsLightSupported(uint8 LightType, ECastRayTracedShadow::Type CastRayTracedShadow);
	bool UseHardwareRayTracing(const FSceneViewFamily& ViewFamily);
	bool UseInlineHardwareRayTracing(const FSceneViewFamily& ViewFamily);
	bool ShouldCompileShaders(const FGlobalShaderPermutationParameters& Parameters);
};