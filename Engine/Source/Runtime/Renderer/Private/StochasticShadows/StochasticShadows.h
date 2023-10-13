// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

namespace ECastRayTracedShadow
{
	enum Type : int;
};

namespace StochasticShadows
{
	bool IsEnabled();
	bool IsUsingClosestHZB();
	bool IsLightSupported(uint8 LightType, ECastRayTracedShadow::Type CastRayTracedShadow);
	bool UseHardwareRayTracing();
};