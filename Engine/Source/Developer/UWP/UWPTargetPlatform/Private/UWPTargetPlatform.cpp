
/*=============================================================================
	UWPTargetPlatform.cpp: Implements the FUWPTargetPlatform class.
=============================================================================*/

#include "UWPTargetPlatform.h"
#include "UWPTargetDevice.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/ScopeLock.h"
#include "HttpModule.h"
#include "PlatformHttp.h"
#include "Interfaces/IHttpResponse.h"

DEFINE_LOG_CATEGORY_STATIC(LogUWPTargetPlatform, Log, All);

FUWPTargetPlatform::FUWPTargetPlatform(const FName& InPlatformName)
	: TTargetPlatformBase(InPlatformName)
	, UWPDeviceDetectorModule(IUWPDeviceDetectorModule::Get())
{
#if WITH_ENGINE
	FConfigCacheIni::LoadLocalIniFile(EngineSettings, TEXT("Engine"), true, *PlatformName());
	TextureLODSettings = nullptr; // These are registered by the device profile system.
	StaticMeshLODSettings.Initialize(EngineSettings);
#endif

	DeviceDetectedRegistration = IUWPDeviceDetectorModule::Get().OnDeviceDetected().AddRaw(this, &FUWPTargetPlatform::OnDeviceDetected);

}

FUWPTargetPlatform::~FUWPTargetPlatform()
{
	IUWPDeviceDetectorModule::Get().OnDeviceDetected().Remove(DeviceDetectedRegistration);
}

void FUWPTargetPlatform::GetAllDevices(TArray<ITargetDevicePtr>& OutDevices) const
{
	UWPDeviceDetectorModule.StartDeviceDetection();

	OutDevices.Reset();
	FScopeLock Lock(&DevicesLock);
	OutDevices = Devices;
}

ITargetDevicePtr FUWPTargetPlatform::GetDevice(const FTargetDeviceId& DeviceId)
{
	if (PlatformName() == DeviceId.GetPlatformName())
	{
		IUWPDeviceDetectorModule::Get().StartDeviceDetection();

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
	IUWPDeviceDetectorModule::Get().StartDeviceDetection();

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

const struct FPlatformAudioCookOverrides* FUWPTargetPlatform::GetAudioCompressionSettings() const
{
	return nullptr;
}

void FUWPTargetPlatform::GetTextureFormats(const UTexture* InTexture, TArray< TArray<FName> >& OutFormats) const
{
	GetDefaultTextureFormatNamePerLayer(OutFormats.AddDefaulted_GetRef(), this, InTexture, EngineSettings, true);
}

void FUWPTargetPlatform::GetAllTextureFormats(TArray<FName>& OutFormats) const
{
	GetAllDefaultTextureFormats(this, OutFormats, true);
}

static FName NAME_PCD3D_SM4(TEXT("PCD3D_SM4"));
static FName NAME_PCD3D_SM5(TEXT("PCD3D_SM5"));

void FUWPTargetPlatform::GetAllPossibleShaderFormats(TArray<FName>& OutFormats) const
{
	OutFormats.AddUnique(NAME_PCD3D_SM5);
}

void FUWPTargetPlatform::GetAllTargetedShaderFormats(TArray<FName>& OutFormats) const
{
	OutFormats.AddUnique(NAME_PCD3D_SM5);
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
