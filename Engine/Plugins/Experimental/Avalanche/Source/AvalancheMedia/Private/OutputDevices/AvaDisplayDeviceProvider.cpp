// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaDisplayDeviceProvider.h"
#include "MediaIOCoreCommonDisplayMode.h"
#include "OutputDevices/AvaDisplayDeviceManager.h"

#define LOCTEXT_NAMESPACE "AvaDisplayDeviceProvider"

FName FAvaDisplayDeviceProvider::GetProviderName()
{
	static FName NAME_Provider = "AvaDisplay";
	return NAME_Provider;
}


FName FAvaDisplayDeviceProvider::GetProtocolName()
{
	static FName NAME_Protocol = "avadisplay";
	return NAME_Protocol;
}

FName FAvaDisplayDeviceProvider::GetFName()
{
	return GetProviderName();
}

TArray<FMediaIOConnection> FAvaDisplayDeviceProvider::GetConnections() const
{
	return TArray<FMediaIOConnection>();
}

TArray<FMediaIOConfiguration> FAvaDisplayDeviceProvider::GetConfigurations() const
{
	return GetConfigurations(true, true);
}

TArray<FMediaIOConfiguration> FAvaDisplayDeviceProvider::GetConfigurations(bool /*bInAllowInput*/, bool bInAllowOutput) const
{
	TArray<FMediaIOConfiguration> Results;
	if (bInAllowOutput)
	{
		TArray<FAvaMonitorInfo> MonitorInfos = FAvaDisplayDeviceManager::GetCachedMonitors();

		for (int32 i = 0; i < MonitorInfos.Num(); ++i)
		{
			const FAvaMonitorInfo& MonitorInfo = MonitorInfos[i];

			FMediaIOConfiguration MediaConfiguration = GetDefaultConfiguration();
			MediaConfiguration.bIsInput = false;
			MediaConfiguration.MediaMode.Resolution = FIntPoint(MonitorInfo.Width, MonitorInfo.Height);
			if (MonitorInfo.DisplayFrequency.IsValid())
			{
				MediaConfiguration.MediaMode.FrameRate = MonitorInfo.DisplayFrequency;
			}

			MediaConfiguration.MediaConnection.Device.DeviceName = *FAvaDisplayDeviceManager::GetMonitorDisplayName(MonitorInfo);
			MediaConfiguration.MediaConnection.Device.DeviceIdentifier = i;
			MediaConfiguration.MediaConnection.Protocol = GetProtocolName();
			MediaConfiguration.MediaConnection.PortIdentifier = 0;

			Results.Add(MediaConfiguration);
		}
	}
	return Results;
}

TArray<FMediaIOInputConfiguration> FAvaDisplayDeviceProvider::GetInputConfigurations() const
{
	return TArray<FMediaIOInputConfiguration>();
}

TArray<FMediaIOOutputConfiguration> FAvaDisplayDeviceProvider::GetOutputConfigurations() const
{
	TArray<FMediaIOOutputConfiguration> Results;

	TArray<FMediaIOConfiguration> Configs = GetConfigurations(false, true);

	FMediaIOOutputConfiguration DefaultOutputConfiguration = GetDefaultOutputConfiguration();
	DefaultOutputConfiguration.KeyPortIdentifier = 0;
	DefaultOutputConfiguration.OutputType = EMediaIOOutputType::Fill;	// HDMI/Display port don't support alpha.
	DefaultOutputConfiguration.OutputReference = EMediaIOReferenceType::FreeRun;

	for (const FMediaIOConfiguration& Config : Configs)
	{
		DefaultOutputConfiguration.MediaConfiguration = Config;
		Results.Add(DefaultOutputConfiguration);
	}
	return Results;
}

TArray<FMediaIOVideoTimecodeConfiguration> FAvaDisplayDeviceProvider::GetTimecodeConfigurations() const
{
	TArray<FMediaIOVideoTimecodeConfiguration> MediaConfigurations;
	return MediaConfigurations;
}

TArray<FMediaIODevice> FAvaDisplayDeviceProvider::GetDevices() const
{
	TArray<FMediaIODevice> Results;

	TArray<FAvaMonitorInfo> MonitorInfos = FAvaDisplayDeviceManager::GetCachedMonitors();
	
	for (int32 i = 0; i < MonitorInfos.Num(); ++i)
	{
		const FAvaMonitorInfo& MonitorInfo = MonitorInfos[i];
		FMediaIODevice Device;
		Device.DeviceName = *FAvaDisplayDeviceManager::GetMonitorDisplayName(MonitorInfo);
		Device.DeviceIdentifier = i;
		Results.Add(Device);
	}

	return Results;
}


TArray<FMediaIOMode> FAvaDisplayDeviceProvider::GetModes(const FMediaIODevice& InDevice, bool bInOutput) const
{
	TArray<FMediaIOMode> Results;
	return Results;
}

FMediaIOConfiguration FAvaDisplayDeviceProvider::GetDefaultConfiguration() const
{
	FMediaIOConfiguration Configuration;
	Configuration.bIsInput = true;
	Configuration.MediaConnection.Device.DeviceIdentifier = 1;
	Configuration.MediaConnection.Protocol = GetProtocolName();
	Configuration.MediaConnection.PortIdentifier = 0;
	Configuration.MediaMode = GetDefaultMode();
	return Configuration;
}


FMediaIOMode FAvaDisplayDeviceProvider::GetDefaultMode() const
{
	FMediaIOMode Mode;
	Mode.DeviceModeIdentifier = 0;	// Unused, but can't be invalid.
	Mode.FrameRate = FFrameRate(30, 1);
	Mode.Resolution = FIntPoint(1920, 1080);
	Mode.Standard = EMediaIOStandardType::Progressive;
	return Mode;
}


FMediaIOInputConfiguration FAvaDisplayDeviceProvider::GetDefaultInputConfiguration() const
{
	FMediaIOInputConfiguration Configuration;
	Configuration.MediaConfiguration = GetDefaultConfiguration();
	Configuration.MediaConfiguration.bIsInput = true;
	Configuration.InputType = EMediaIOInputType::Fill;
	return Configuration;
}

FMediaIOOutputConfiguration FAvaDisplayDeviceProvider::GetDefaultOutputConfiguration() const
{
	FMediaIOOutputConfiguration Configuration;
	Configuration.MediaConfiguration = GetDefaultConfiguration();
	Configuration.MediaConfiguration.bIsInput = false;
	Configuration.OutputReference = EMediaIOReferenceType::FreeRun;
	Configuration.OutputType = EMediaIOOutputType::Fill;
	return Configuration;
}

FMediaIOVideoTimecodeConfiguration FAvaDisplayDeviceProvider::GetDefaultTimecodeConfiguration() const
{
	FMediaIOVideoTimecodeConfiguration Configuration;
	Configuration.MediaConfiguration = GetDefaultConfiguration();
	return Configuration;
}

FText FAvaDisplayDeviceProvider::ToText(const FMediaIOConfiguration& InConfiguration, bool bInIsAutoDetected) const
{
	if (bInIsAutoDetected)
	{
		return FText::Format(LOCTEXT("FMediaIOAutoConfigurationToText", "{0} - {1} [device{2}/auto]")
				, InConfiguration.bIsInput ? LOCTEXT("In", "In") : LOCTEXT("Out", "Out")
				, FText::FromName(InConfiguration.MediaConnection.Device.DeviceName)
				, FText::AsNumber(InConfiguration.MediaConnection.Device.DeviceIdentifier)
				);
	}
	if (InConfiguration.IsValid())
	{
		return FText::Format(LOCTEXT("FMediaIOConfigurationToText", "[{0}] - {1} [device{2}/{3}]")
			, InConfiguration.bIsInput ? LOCTEXT("In", "In") : LOCTEXT("Out", "Out")
			, FText::FromName(InConfiguration.MediaConnection.Device.DeviceName)
			, FText::AsNumber(InConfiguration.MediaConnection.Device.DeviceIdentifier)
			, InConfiguration.MediaMode.GetModeName()
		);
	}
	return LOCTEXT("Invalid", "<Invalid>");
}


FText FAvaDisplayDeviceProvider::ToText(const FMediaIOConnection& InConnection) const
{
	if (InConnection.IsValid())
	{
		return FText::Format(LOCTEXT("FMediaIOConnectionToText", "{0} [device{1}]")
			, FText::FromName(InConnection.Device.DeviceName)
			, LOCTEXT("Device", "device")
			, FText::AsNumber(InConnection.Device.DeviceIdentifier)
		);
	}
	return LOCTEXT("Invalid", "<Invalid>");
}

FText FAvaDisplayDeviceProvider::ToText(const FMediaIOOutputConfiguration& InConfiguration) const
{
	if (InConfiguration.IsValid())
	{
		return FText::Format(LOCTEXT("FMediaIOOutputConfigurationToText", "{0} - {1} [device{2}/{3}/{4}]")
			, InConfiguration.OutputType == EMediaIOOutputType::Fill ? LOCTEXT("Fill", "Fill") : LOCTEXT("FillAndKey", "Fill&Key")
			, FText::FromName(InConfiguration.MediaConfiguration.MediaConnection.Device.DeviceName)
			, FText::AsNumber(InConfiguration.MediaConfiguration.MediaConnection.Device.DeviceIdentifier)
			, GetTransportName(InConfiguration.MediaConfiguration.MediaConnection.TransportType, InConfiguration.MediaConfiguration.MediaConnection.QuadTransportType)
			, InConfiguration.MediaConfiguration.MediaMode.GetModeName()
		);
	}
	return LOCTEXT("Invalid", "<Invalid>");
}

#undef LOCTEXT_NAMESPACE
