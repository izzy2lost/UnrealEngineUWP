// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"
#include "UObject/ObjectMacros.h"

UENUM(BlueprintType)
enum class EDMTextureChannelMask : uint8
{
	Red = 0,
	Green = 1 << 0,
	Blue = 1 << 1,
	Alpha = 1 << 2,
	RGB = Red|Green|Blue,
	RGBA = Red|Green|Blue|Alpha,
};
ENUM_CLASS_FLAGS(EDMTextureChannelMask)
