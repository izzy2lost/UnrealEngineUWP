// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "CoreTypes.h"

#ifndef USE_ANDROID_OPENGL_SWAPPY
#define USE_ANDROID_OPENGL_SWAPPY 0
#endif
#ifndef USE_ANDROID_VULKAN_SWAPPY
#define USE_ANDROID_VULKAN_SWAPPY 0
#endif

#if USE_ANDROID_OPENGL_SWAPPY || USE_ANDROID_VULKAN_SWAPPY
void LoadSwappy();
void UnloadSwappy();
#endif