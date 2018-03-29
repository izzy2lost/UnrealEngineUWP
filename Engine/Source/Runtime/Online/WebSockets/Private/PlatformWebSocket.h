// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"

#if PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX || PLATFORM_PS4
#include "Lws/LwsWebSocketsManager.h"
typedef FLwsWebSocketsManager FPlatformWebSocketsManager;
#elif PLATFORM_XBOXONE
#include "XboxOne/XboxOneWebSocketsManager.h"
typedef FXboxOneWebSocketsManager FPlatformWebSocketsManager;
// @ATG_CHANGE : BEGIN UWP websockets
#elif PLATFORM_UWP
#include "UWP/UWPWebSocketsManager.h"
typedef FUWPWebSocketsManager FPlatformWebSocketsManager;
#else
// @ATG_CHANGE : END
#error "Web sockets not implemented on this platform yet"
#endif
