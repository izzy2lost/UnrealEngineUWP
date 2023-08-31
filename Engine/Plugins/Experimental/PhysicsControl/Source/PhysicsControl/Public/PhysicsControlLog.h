// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Logging/LogMacros.h"

#if UE_BUILD_SHIPPING || UE_BUILD_TEST
PHYSICSCONTROL_API DECLARE_LOG_CATEGORY_EXTERN(LogRigidBodyWithControl, Log, Warning);
PHYSICSCONTROL_API DECLARE_LOG_CATEGORY_EXTERN(LogPhysicsControlComponent, Log, Warning);
#else
PHYSICSCONTROL_API DECLARE_LOG_CATEGORY_EXTERN(LogRigidBodyWithControl, Log, All);
PHYSICSCONTROL_API DECLARE_LOG_CATEGORY_EXTERN(LogPhysicsControlComponent, Log, All);
#endif


