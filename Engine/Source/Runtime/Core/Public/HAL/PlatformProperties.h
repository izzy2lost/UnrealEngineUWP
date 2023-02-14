// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreTypes.h"
#include "GenericPlatform/GenericPlatformProperties.h"

// note that this is not defined to 1 like normal, because we don't want to have to define it to 0 whenever
// the Properties.h files are included in all other places, so just use #ifdef not #if in this special case
#define PROPERTY_HEADER_SHOULD_DEFINE_TYPE

#if PLATFORM_WINDOWS
#include "Windows/WindowsPlatformProperties.h"
typedef FWindowsPlatformProperties<WITH_EDITORONLY_DATA, UE_SERVER, !WITH_SERVER_CODE> FPlatformProperties;
#elif PLATFORM_PS4
#include "PS4/PS4Properties.h"
typedef FPS4PlatformProperties FPlatformProperties;
#elif PLATFORM_XBOXONE
#include "XboxOne/XboxOneProperties.h"
typedef FXboxOnePlatformProperties FPlatformProperties;
#elif PLATFORM_MAC
#include "Mac/MacPlatformProperties.h"
typedef FMacPlatformProperties<WITH_EDITORONLY_DATA, UE_SERVER, !WITH_SERVER_CODE> FPlatformProperties;
#elif PLATFORM_IOS
#include "IOS/IOSPlatformProperties.h"
typedef FIOSPlatformProperties FPlatformProperties;
#elif PLATFORM_LUMIN
#include "Lumin/LuminPlatformProperties.h"
typedef FLuminPlatformProperties FPlatformProperties;
#elif PLATFORM_ANDROID
#include "Android/AndroidProperties.h"
typedef FAndroidPlatformProperties FPlatformProperties;
// @ATG_CHANGE : BEGIN UWP support
#elif PLATFORM_UWP
#include "UWP/UWPProperties.h"
typedef FUWPPlatformProperties FPlatformProperties;
// @ATG_CHANGE : END
#elif PLATFORM_HTML5
#include "HTML5/HTML5PlatformProperties.h"
typedef FHTML5PlatformProperties FPlatformProperties;
#elif PLATFORM_QUAIL
#include "Quail/QuailPlatformProperties.h"
typedef FQuailPlatformProperties FPlatformProperties;
#elif PLATFORM_LINUX
#include "Linux/LinuxPlatformProperties.h"
typedef FLinuxPlatformProperties<WITH_EDITORONLY_DATA, UE_SERVER, !WITH_SERVER_CODE> FPlatformProperties;
#elif PLATFORM_SWITCH
#include "Switch/SwitchPlatformProperties.h"
typedef FSwitchPlatformProperties FPlatformProperties;
#endif

#undef PROPERTY_HEADER_SHOULD_DEFINE_TYPE
