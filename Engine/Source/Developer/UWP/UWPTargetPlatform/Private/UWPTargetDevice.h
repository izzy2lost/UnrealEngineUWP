
/*=============================================================================
	UWPTargetDevice.h: Declares the UWPTargetDevice class.
=============================================================================*/

#pragma once

#include "AllowWindowsPlatformTypes.h"


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
	FUWPTargetDevice(const ITargetPlatform& InTargetPlatform)
		: TargetPlatform(InTargetPlatform)
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
		return ETargetDeviceTypes::Indeterminate;
	}

	virtual FTargetDeviceId GetId() const override
	{
		return FTargetDeviceId(TargetPlatform.PlatformName(), GetName());
	}

	virtual FString GetName() const override
	{
		return FString(FPlatformProcess::ComputerName()) + TEXT("_UWP");
	}

	virtual FString GetOperatingSystemName() override
	{
		return TEXT("UWP");
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
		return true;
	}

	virtual bool Launch(const FString& AppId, EBuildConfigurations::Type BuildConfiguration, EBuildTargets::Type BuildTarget, const FString& Params, uint32* OutProcessId) override;

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

	virtual bool TerminateProcess(const int32 ProcessId) override
	{
		return false;
	}

private:
	// Holds a reference to the device's target platform.
	const ITargetPlatform& TargetPlatform;
};

#include "HideWindowsPlatformTypes.h"
