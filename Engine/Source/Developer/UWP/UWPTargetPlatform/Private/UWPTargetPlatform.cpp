
/*=============================================================================
	UWPTargetPlatform.cpp: Implements the FUWPTargetPlatform class.
=============================================================================*/

#include "UWPTargetPlatformPrivatePCH.h"

DEFINE_LOG_CATEGORY_STATIC(LogUWPTargetPlatform, Log, All);

#include "AllowWindowsPlatformTypes.h"

FUWPTargetPlatform::FUWPTargetPlatform()
{
	LocalDevice = MakeShareable(new FUWPTargetDevice(*this));

#if WITH_ENGINE
	FConfigCacheIni::LoadLocalIniFile(EngineSettings, TEXT("Engine"), true, *PlatformName());
	TextureLODSettings = nullptr; // These are registered by the device profile system.
	StaticMeshLODSettings.Initialize(EngineSettings);
#endif
}

void FUWPTargetPlatform::GetAllDevices(TArray<ITargetDevicePtr>& OutDevices) const
{
	OutDevices.Reset();
	OutDevices.Add(LocalDevice);
}

ITargetDevicePtr FUWPTargetPlatform::GetDevice(const FTargetDeviceId& DeviceId)
{
	if (LocalDevice.IsValid() && (DeviceId == LocalDevice->GetId()))
	{
		return LocalDevice;
	}

	return nullptr;
}

ITargetDevicePtr FUWPTargetPlatform::GetDefaultDevice() const
{
	return LocalDevice;
}

bool FUWPTargetPlatform::SupportsFeature(ETargetPlatformFeatures Feature) const
{
	if (Feature == ETargetPlatformFeatures::Packaging)
	{
		return true;
	}

	return TTargetPlatformBase<FUWPPlatformProperties>::SupportsFeature(Feature);
}

#include "HideWindowsPlatformTypes.h"
