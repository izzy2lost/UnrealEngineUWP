// Copyright Epic Games, Inc. All Rights Reserved.

#pragma  once

#include "GenericPlatform/GenericPlatform.h"
#include "Misc/EnumClassFlags.h"

enum class ERCModifyOperationFlags : FGenericPlatformTypes::uint8
{
	None = 0x00,
	SkipPropertyChangeEvents = 0x01
};

ENUM_CLASS_FLAGS(ERCModifyOperationFlags)
