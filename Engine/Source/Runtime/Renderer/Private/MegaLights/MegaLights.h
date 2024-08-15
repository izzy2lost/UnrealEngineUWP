// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FSceneViewFamily;
struct FGlobalShaderPermutationParameters;
enum EShaderPlatform : uint16;
class FRDGTexture;
using FRDGTextureRef = FRDGTexture*;

namespace ECastRayTracedShadow
{
	enum Type : int;
};

class FMegaLightsVolume
{
public:
	FRDGTextureRef Texture = nullptr;
};

// Public MegaLights interface
namespace MegaLights
{
	bool IsEnabled();
	bool IsUsingForcedRaytracing();
	bool IsUsingVirtualShadowMaps();

	bool IsUsingClosestHZB();
	bool IsUsingGlobalSDF(const FSceneViewFamily& ViewFamily);
	bool IsUsingLightFunctions();

	bool IsLightSupported(uint8 LightType, ECastRayTracedShadow::Type CastRayTracedShadow, bool bVSMEnabled);
	bool AllowShadowMaps(uint8 LightType, ECastRayTracedShadow::Type CastRayTracedShadow);
	bool UseHardwareRayTracing(const FSceneViewFamily& ViewFamily);
	bool UseInlineHardwareRayTracing(const FSceneViewFamily& ViewFamily);
	bool ShouldCompileShaders(EShaderPlatform ShaderPlatform);
};