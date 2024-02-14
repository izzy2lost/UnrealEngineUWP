// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if PLATFORM_WINDOWS
	#include "Windows/AllowWindowsPlatformTypes.h"
	THIRD_PARTY_INCLUDES_START
		#include <GL/glcorearb.h>
		#include <GL/glext.h>
		#include <GL/wglext.h>
	THIRD_PARTY_INCLUDES_END
	#include "Windows/HideWindowsPlatformTypes.h"
#elif PLATFORM_LINUX
	THIRD_PARTY_INCLUDES_START
		#include <GL/glcorearb.h>
		#include <GL/glext.h>
	THIRD_PARTY_INCLUDES_END
#elif PLATFORM_ANDROID
	#include "Android/AndroidPlatform.h"
	THIRD_PARTY_INCLUDES_START
		#include <EGL/egl.h>
		#include <EGL/eglext.h>
		#include <GLES3/gl31.h>
	THIRD_PARTY_INCLUDES_END
#endif

using UGLsync = GLsync;
