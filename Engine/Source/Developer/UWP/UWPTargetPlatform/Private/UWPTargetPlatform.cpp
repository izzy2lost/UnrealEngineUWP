
/*=============================================================================
	UWPTargetPlatform.cpp: Implements the FUWPTargetPlatform class.
=============================================================================*/

#include "UWPTargetPlatform.h"
#include "UWPTargetDevice.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/ScopeLock.h"
#include "HttpModule.h"
#include "PlatformHttp.h"
#include "IHttpResponse.h"

DEFINE_LOG_CATEGORY_STATIC(LogUWPTargetPlatform, Log, All);

FUWPTargetPlatform::FUWPTargetPlatform()
{
#if WITH_ENGINE
	FConfigCacheIni::LoadLocalIniFile(EngineSettings, TEXT("Engine"), true, *PlatformName());
	TextureLODSettings = nullptr; // These are registered by the device profile system.
	StaticMeshLODSettings.Initialize(EngineSettings);
#endif

	DeviceDetectedRegistration = IUWPDeviceDetectorModule::Get().OnDeviceDetected().AddRaw(this, &FUWPTargetPlatform::OnDeviceDetected);

	IUWPDeviceDetectorModule::Get().StartDeviceDetection();
}

FUWPTargetPlatform::~FUWPTargetPlatform()
{
	IUWPDeviceDetectorModule::Get().OnDeviceDetected().Remove(DeviceDetectedRegistration);
}

void FUWPTargetPlatform::GetAllDevices(TArray<ITargetDevicePtr>& OutDevices) const
{
	OutDevices.Reset();
	FScopeLock Lock(&DevicesLock);
	OutDevices = Devices;
}

ITargetDevicePtr FUWPTargetPlatform::GetDevice(const FTargetDeviceId& DeviceId)
{
	if (PlatformName() == DeviceId.GetPlatformName())
	{
		FScopeLock Lock(&DevicesLock);
		for (ITargetDevicePtr Device : Devices)
		{
			if (DeviceId == Device->GetId())
			{
				return Device;
			}
		}
	}


	return nullptr;
}

ITargetDevicePtr FUWPTargetPlatform::GetDefaultDevice() const
{
	FScopeLock Lock(&DevicesLock);
	for (ITargetDevicePtr RemoteDevice : Devices)
	{
		if (RemoteDevice->IsDefault())
		{
			return RemoteDevice;
		}
	}

	return nullptr;
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

void FUWPTargetPlatform::OnDeviceDetected(const FUWPDeviceInfo& Info)
{
	if (SupportsDevice(Info.DeviceTypeName, Info.Is64Bit != 0))
	{
		// Don't automatically add remote devices that require credentials.  They
		// must be manually added by the user so that we can collect those credentials.
		if (Info.IsLocal() || !Info.RequiresCredentials)
		{
			FUWPDevicePtr NewDevice = MakeShared<FUWPTargetDevice, ESPMode::ThreadSafe>(*this, Info);
			{
				FScopeLock Lock(&DevicesLock);
				Devices.Add(NewDevice);
			}
			DeviceDiscoveredEvent.Broadcast(NewDevice.ToSharedRef());
		}
	}
}
