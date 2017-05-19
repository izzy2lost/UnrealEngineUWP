
/*=============================================================================
	UWPTargetPlatform.cpp: Implements the FUWPTargetPlatform class.
=============================================================================*/

#include "UWPTargetPlatform.h"
#include "UWPTargetDevice.h"
#include "Misc/ConfigCacheIni.h"

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

void FUWPTargetPlatform::GetTextureFormats(const UTexture* InTexture, TArray<FName>& OutFormats) const
{
	bool bExcludeShaderModel4Support = false;
	GConfig->GetBool(TEXT("/Script/UWPTargetPlatform.UWPTargetSettings"), TEXT("bExcludeShaderModel4Support"), bExcludeShaderModel4Support, GEngineIni);
	FName TextureFormatName = GetDefaultTextureFormatName(this, InTexture, EngineSettings, bExcludeShaderModel4Support);
	OutFormats.Add(TextureFormatName);
}

#include "HideWindowsPlatformTypes.h"
