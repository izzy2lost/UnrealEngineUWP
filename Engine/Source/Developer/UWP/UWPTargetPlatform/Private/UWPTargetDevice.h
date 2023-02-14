
/*=============================================================================
	UWPTargetDevice.h: Declares the UWPTargetDevice class.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "HAL/PlatformProcess.h"
#include "Interfaces/ITargetDevice.h"
#include "Interfaces/ITargetPlatform.h"
#include "IUWPDeviceDetectorModule.h"

#include "Windows/AllowWindowsPlatformTypes.h"

/**
 * Implements a UWP target device.
 */
class FUWPTargetDevice
	: public ITargetDevice
{
public:

	/**
	* Creates and initializes a new device for the specified target platform.
	*
	* @param InTargetPlatform - The target platform.
	*/
	FUWPTargetDevice(const ITargetPlatform& InTargetPlatform, const FUWPDeviceInfo& InInfo)
		: TargetPlatform(InTargetPlatform)
		, Info(InInfo)
	{ }


	virtual bool Connect() override
	{
		return true;
	}

	virtual bool Deploy(const FString& SourceFolder, FString& OutAppId) override;

	virtual void Disconnect() override
	{ }

	virtual ETargetDeviceTypes GetDeviceType() const override
	{
		if (Info.DeviceTypeName == UWPDeviceTypes::Desktop)
		{
			return ETargetDeviceTypes::Desktop;
		}
		else if (Info.DeviceTypeName == UWPDeviceTypes::Xbox)
		{
			return ETargetDeviceTypes::Console;
		}
		else
		{
			return ETargetDeviceTypes::Indeterminate;
		}
	}

	virtual FTargetDeviceId GetId() const override
	{
		if (Info.IsLocal())
		{
			return FTargetDeviceId(TargetPlatform.PlatformName(), Info.HostName);
		}
		else
		{
			// This is what gets handed off to UAT, so we need to supply the
			// actual Device Portal url instead of just the host name
			return FTargetDeviceId(TargetPlatform.PlatformName(), Info.WdpUrl);
		}
	}

	virtual FString GetName() const override
	{
		return Info.HostName + TEXT(" (UWP)");
	}

	virtual FString GetOperatingSystemName() override
	{
		return FString::Printf(TEXT("UWP (%s)"), *Info.DeviceTypeName.ToString());
	}

	virtual int32 GetProcessSnapshot(TArray<FTargetDeviceProcessInfo>& OutProcessInfos) override
	{
		return 0;
	}

	virtual const class ITargetPlatform& GetTargetPlatform() const override
	{
		return TargetPlatform;
	}

	virtual bool GetUserCredentials(FString& OutUserName, FString& OutUserPassword) override
	{
		return false;
	}

	virtual bool IsConnected()
	{
		return true;
	}

	virtual bool IsDefault() const override
	{
		return Info.HostName == FPlatformProcess::ComputerName();
	}

	virtual bool Launch(const FString& AppId, EBuildConfiguration BuildConfiguration, EBuildTargetType BuildTarget, const FString& Params, uint32* OutProcessId) override;

	virtual bool PowerOff(bool Force) override
	{
		return false;
	}

	virtual bool PowerOn() override
	{
		return false;
	}

	virtual bool Reboot(bool bReconnect = false) override
	{
		return false;
	}

	virtual bool Run(const FString& ExecutablePath, const FString& Params, uint32* OutProcessId) override;

	virtual void SetUserCredentials( const FString& UserName, const FString& UserPassword ) override { }

	virtual bool SupportsFeature(ETargetDeviceFeatures Feature) const override
	{
		return false;
	}

	virtual bool SupportsSdkVersion(const FString& VersionString) const override
	{
		return false;
	}

	virtual bool TerminateProcess(const int64 ProcessId) override
	{
		return false;
	}

private:
	// Holds a reference to the device's target platform.
	const ITargetPlatform& TargetPlatform;

	FUWPDeviceInfo Info;
};

typedef TSharedPtr<FUWPTargetDevice, ESPMode::ThreadSafe> FUWPDevicePtr;

#include "Windows/HideWindowsPlatformTypes.h"
