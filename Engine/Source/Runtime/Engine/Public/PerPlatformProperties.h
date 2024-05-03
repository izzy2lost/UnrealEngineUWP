// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
Redirect PerPlatformProperties.h include to UObject
=============================================================================*/

#pragma once

#pragma message("WARNING: Do not #include PerPlatformProperties.h. Please use #include UObject/PerPlatformProperties.h instead. Deprecated in 5.5")

#include "UObject/PerPlatformProperties.h"
