// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Logging/LogMacros.h"

#if UE_BUILD_SHIPPING || UE_BUILD_TEST
DECLARE_LOG_CATEGORY_EXTERN(LogRigidBodyWithControl, Log, Warning);
DECLARE_LOG_CATEGORY_EXTERN(LogPhysicsControlComponent, Log, Warning);
#else
DECLARE_LOG_CATEGORY_EXTERN(LogRigidBodyWithControl, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogPhysicsControlComponent, Log, All);
#endif


