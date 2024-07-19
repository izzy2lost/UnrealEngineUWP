// Copyright Epic Games, Inc. All Rights Reserved.

#include "Modules/ModuleManager.h"
#include "Windows/WindowsPlatformMisc.h"

#if PLATFORM_WINDOWS

#include "Windows/AllowWindowsPlatformTypes.h"

THIRD_PARTY_INCLUDES_START
// Linkage to define  Windows PKEY GUIDs included by Notification/DeviceInfoCache, otherwise they are unresolved extern.
#include <initguid.h>
THIRD_PARTY_INCLUDES_END

#include "Windows/HideWindowsPlatformTypes.h"

#endif // PLATFORM_WINDOWS

#include "WindowsMMNotificationClient.h"
#include "WindowsMMDeviceInfoCache.h"
#include "WindowsMMDeviceEnumerationLog.h"

DEFINE_LOG_CATEGORY(LogAudioEnumeration);

namespace Audio
{
	class FWindowsMMDeviceEnumerationModule : public IModuleInterface
	{
	private:
		/** Indicates if FWindowsPlatformMisc::CoInitialize() was successfull. */
		bool bCoInitialized = false;

	public:
		virtual void StartupModule() override
		{
			bCoInitialized = FWindowsPlatformMisc::CoInitialize();
		}

		virtual void ShutdownModule() override
		{
			if (bCoInitialized)
			{
				FWindowsPlatformMisc::CoUninitialize();
			}
		}
	};
}

IMPLEMENT_MODULE(Audio::FWindowsMMDeviceEnumerationModule, WindowsMMDeviceEnumeration)
