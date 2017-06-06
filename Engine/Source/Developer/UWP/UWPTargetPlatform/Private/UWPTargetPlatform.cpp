
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

#if WITH_ENGINE

void FUWPTargetPlatform::GetTextureFormats(const UTexture* InTexture, TArray<FName>& OutFormats) const
{
	bool bExcludeShaderModel4Support = false;
	GConfig->GetBool(TEXT("/Script/UWPPlatformEditor.UWPTargetSettings"), TEXT("bExcludeShaderModel4Support"), bExcludeShaderModel4Support, GEngineIni);
	FName TextureFormatName = GetDefaultTextureFormatName(this, InTexture, EngineSettings, bExcludeShaderModel4Support);
	OutFormats.Add(TextureFormatName);
}

static FName NAME_PCD3D_SM4(TEXT("PCD3D_SM4"));
static FName NAME_PCD3D_SM5(TEXT("PCD3D_SM5"));

void FUWPTargetPlatform::GetAllPossibleShaderFormats(TArray<FName>& OutFormats) const
{
	OutFormats.AddUnique(NAME_PCD3D_SM5);
	OutFormats.AddUnique(NAME_PCD3D_SM4);
}

void FUWPTargetPlatform::GetAllTargetedShaderFormats(TArray<FName>& OutFormats) const
{
	bool bExcludeShaderModel4Support = false;
	GConfig->GetBool(TEXT("/Script/UWPPlatformEditor.UWPTargetSettings"), TEXT("bExcludeShaderModel4Support"), bExcludeShaderModel4Support, GEngineIni);
	OutFormats.AddUnique(NAME_PCD3D_SM5);
	if (!bExcludeShaderModel4Support)
	{
		OutFormats.AddUnique(NAME_PCD3D_SM4);
	}
}

#endif


#include "HideWindowsPlatformTypes.h"
