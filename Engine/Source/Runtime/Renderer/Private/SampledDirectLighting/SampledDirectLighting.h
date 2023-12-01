// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

namespace ECastRayTracedShadow
{
	enum Type : int;
};

// Public SampledDirectLighting interface
namespace SampledDirectLighting
{
	bool IsEnabled();
	bool IsUsingClosestHZB();
	bool IsLightSupported(uint8 LightType, ECastRayTracedShadow::Type CastRayTracedShadow);
	bool UseHardwareRayTracing();
	bool UseInlineHardwareRayTracing();
	bool UseGlobalSDF();
};