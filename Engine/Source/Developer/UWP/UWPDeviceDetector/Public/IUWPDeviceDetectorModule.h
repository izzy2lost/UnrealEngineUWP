#pragma once

#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "HAL/PlatformProcess.h"

namespace UWPDeviceTypes
{
	extern UWPDEVICEDETECTOR_API const FName Desktop;
	extern UWPDEVICEDETECTOR_API const FName Xbox;
}

struct FUWPDeviceInfo
{
	FString HostName;
	FString WdpUrl;
	FString WindowsDeviceId;
	FName DeviceTypeName;
	int32 Is64Bit :1;
	int32 RequiresCredentials : 1;

	bool IsLocal() const
	{
		return HostName == FPlatformProcess::ComputerName();
	}
};

class IUWPDeviceDetectorModule :
	public IModuleInterface
{
public:
	static inline IUWPDeviceDetectorModule& Get()
	{
		return FModuleManager::LoadModuleChecked<IUWPDeviceDetectorModule>("UWPDeviceDetector");
	}

	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("UWPDeviceDetector");
	}

public:
	virtual void StartDeviceDetection() = 0;
	virtual void StopDeviceDetection() = 0;

	DECLARE_EVENT_OneParam(IUWPDeviceDetectorModule, FOnDeviceDetected, const FUWPDeviceInfo&);
	virtual FOnDeviceDetected& OnDeviceDetected() = 0;

	virtual const TArray<FUWPDeviceInfo> GetKnownDevices() = 0;
};

