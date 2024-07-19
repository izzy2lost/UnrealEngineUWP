// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AudioMixer.h"

#include "Windows/AllowWindowsPlatformTypes.h"


THIRD_PARTY_INCLUDES_START
#include <winerror.h>

#if PLATFORM_WINDOWS
#include <audiopolicy.h>			// IAudioSessionEvents
#endif //PLATFORM_WINDOWS

THIRD_PARTY_INCLUDES_END

#include "Windows/HideWindowsPlatformTypes.h"

namespace Audio
{
	FString ToErrorFString(HRESULT InResult);
}
