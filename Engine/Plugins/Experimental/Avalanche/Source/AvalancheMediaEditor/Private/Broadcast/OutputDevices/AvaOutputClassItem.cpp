// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaOutputClassItem.h"
#include "AvaOutputDeviceItem.h"
#include "AvaOutputServerItem.h"
#include "AvalancheBroadcast.h"
#include "IAvaMediaModule.h"
#include "IMediaIOCoreDeviceProvider.h"
#include "IMediaIOCoreModule.h"
#include "MediaIOCoreDefinitions.h"
#include "MediaOutput.h"
#include "OutputDevices/AvaDeviceProviderData.h"
#include "OutputDevices/AvaMediaOutputUtils.h"
#include "OutputDevices/IAvaDeviceProviderProxyManager.h"
#include "ScopedTransaction.h"
#include "Slate/SAvaOutputTreeItem.h"
#include "Styling/SlateIconFinder.h"

#define LOCTEXT_NAMESPACE "AvaOutputClassItem"

namespace UE::AvaOutputClassItem::Private
{
	IMediaIOCoreDeviceProvider* GetDeviceProvider(const UClass* InMediaOutputClass)
	{
		const FName DeviceProviderName = UE::AvaMediaOutputUtils::GetDeviceProviderName(InMediaOutputClass);
		if (!IMediaIOCoreModule::IsAvailable() || DeviceProviderName.IsNone())
		{
			return nullptr;
		}

		// Remark: even if the class has a device provider name, it is possible
		// that there is no corresponding device provider.
		return IMediaIOCoreModule::Get().GetDeviceProvider(DeviceProviderName);
	}

	void UpdateDeviceName(UAvalancheBroadcast& InBroadcast, const FName& InChannelName, const UMediaOutput* InMediaOutput, const FString& InDeviceName)
	{
		FAvaOutputChannel& Channel = InBroadcast.GetCurrentProfile().GetChannelMutable(InChannelName);
		if (Channel.IsValidChannel())
		{
			if (FAvaMediaOutputInfo* ExistingOutputInfo = Channel.GetMediaOutputInfoMutable(InMediaOutput))
			{
				ExistingOutputInfo->DeviceName = FName(InDeviceName);	
			}
		}
	}
}

FText FAvaOutputClassItem::GetDisplayName() const
{
	check(OutputClass.IsValid());
	return OutputClass->GetDisplayNameText();
}

const FSlateBrush* FAvaOutputClassItem::GetIconBrush() const
{
	check(OutputClass.IsValid());
	return FSlateIconFinder::FindIconBrushForClass(OutputClass.Get());
}

void FAvaOutputClassItem::RefreshChildren()
{
	IMediaIOCoreDeviceProvider* DeviceProvider = UE::AvaOutputClassItem::Private::GetDeviceProvider(OutputClass.Get());

	if (!DeviceProvider)
	{
		Children.Reset();
		return;
	}

	TSharedPtr<FAvaOutputTreeItem> Parent = ParentWeak.Pin();
	if (!Parent.IsValid())
	{
		return;
	}

	TSharedPtr<FAvaOutputServerItem> ParentServerItem = StaticCastSharedPtr<FAvaOutputServerItem>(Parent);
	check(ParentServerItem.IsValid());

	const FName DeviceProviderName = DeviceProvider->GetFName();
	const IAvaDeviceProviderProxyManager& DeviceProviderProxyManager = IAvaMediaModule::Get().GetDeviceProviderProxyManager();

	TArray<FMediaIOConfiguration> OutputConfigs = DeviceProvider->GetConfigurations(false, true);

	// Fill current devices from output configs of this device provider.
	TSet<FAvaMediaDevice> CurrentDevices;
	CurrentDevices.Reserve(OutputConfigs.Num());

	for (const FMediaIOConfiguration& Config : OutputConfigs)
	{
		const FString ServerName = DeviceProviderProxyManager.FindServerNameForDevice(DeviceProviderName, Config.MediaConnection.Device.DeviceName);
		const bool bIsLocal = DeviceProviderProxyManager.IsLocalDevice(DeviceProviderName, Config.MediaConnection.Device.DeviceName);
		const FAvaDeviceProviderData* DeviceProviderDataForServer = ParentServerItem->GetDeviceProviderData(DeviceProviderName);

		if ((!DeviceProviderDataForServer && bIsLocal)
			 || (DeviceProviderDataForServer && !bIsLocal && ServerName == ParentServerItem->GetServerName()))
		{
			CurrentDevices.Add({ Config.MediaConnection.Device, DeviceProviderName, ServerName });
		}
	}

	//A set to contain the Media Devices of the Current Children
	TSet<FAvaMediaDevice> SeenDevices;
	SeenDevices.Reserve(CurrentDevices.Num());
	
	//Remove Existing Children that are Invalid
	for (TArray<FAvaOutputTreeItemPtr>::TIterator ItemIt = Children.CreateIterator(); ItemIt; ++ItemIt)
	{
		FAvaOutputTreeItemPtr Item(*ItemIt);

		//Remove Invalid Pointers or Items that are not Device Items since a Class Item can only contain Device Items
		if (!Item.IsValid() || !Item->IsA<FAvaOutputDeviceItem>())
		{
			ItemIt.RemoveCurrent();
			continue;
		}

		const TSharedPtr<FAvaOutputDeviceItem> DeviceItem = StaticCastSharedPtr<FAvaOutputDeviceItem>(Item);
		const FAvaMediaDevice& UnderlyingDevice = DeviceItem->GetDevice();

		if (UnderlyingDevice.IsValid() && CurrentDevices.Contains(UnderlyingDevice))
		{
			SeenDevices.Add(UnderlyingDevice);
		}
		else
		{
			//Remove if there's no valid Underlying Device or it's no longer in the set
			ItemIt.RemoveCurrent();
		}
	}
	
	//Append the Devices that are not already in the Original Children List
	{
		TSharedPtr<FAvaOutputClassItem> This = SharedThis(this);
		TArray<FAvaMediaDevice> NewDevices = CurrentDevices.Difference(SeenDevices).Array();
		Children.Reserve(Children.Num() + NewDevices.Num());
		
		for (const FAvaMediaDevice& Device : NewDevices)
		{
			TSharedPtr<FAvaOutputDeviceItem> DeviceItem = MakeShared<FAvaOutputDeviceItem>(This, Device);
			Children.Add(DeviceItem);
		}
	}
}

TSharedPtr<SWidget> FAvaOutputClassItem::GenerateRowWidget()
{
	return SNew(SAvaOutputTreeItem, SharedThis(this));
}

UMediaOutput* FAvaOutputClassItem::AddMediaOutputToChannel(FName InTargetChannel, const FAvaMediaOutputInfo& InOutputInfo)
{
	check(OutputClass.IsValid());

	UAvalancheBroadcast& Broadcast = UAvalancheBroadcast::Get();
	
	FScopedTransaction Transaction(LOCTEXT("AddMediaOutput", "Add Media Output"));
	Broadcast.Modify();

	FAvaMediaOutputInfo OutputInfo = InOutputInfo;
	
	// Support non-enumerated devices (i.e. no device provider).
	if (!OutputInfo.IsValid() || !OutputInfo.Guid.IsValid())
	{
		if (TSharedPtr<FAvaOutputTreeItem> Parent = ParentWeak.Pin())
		{
			const TSharedPtr<FAvaOutputServerItem> ParentServerItem = StaticCastSharedPtr<FAvaOutputServerItem>(Parent);
			check(ParentServerItem.IsValid());

			// Fill the device info with available data.
			OutputInfo.Guid = FGuid::NewGuid();
			OutputInfo.ServerName = ParentServerItem->GetServerName();

			// Fetch the device provider name (if there is one) from the class's meta data.
			OutputInfo.DeviceProviderName = UE::AvaMediaOutputUtils::GetDeviceProviderName(OutputClass.Get());

			// Device name:
			// There is no enumerated device name. It will be updated from the media output object below.
		}
	}
	
	UMediaOutput* const MediaOutput = Broadcast.GetCurrentProfile().AddChannelMediaOutput(InTargetChannel, OutputClass.Get(), OutputInfo);

	// If the device name was not provided as input, we need to update it from the Media Output object.
	if (MediaOutput && OutputInfo.DeviceName.IsNone())
	{
		const FString DeviceName = UE::AvaMediaOutputUtils::GetDeviceName(MediaOutput);
		if (!DeviceName.IsEmpty())
		{
			UE::AvaOutputClassItem::Private::UpdateDeviceName(Broadcast, InTargetChannel, MediaOutput, DeviceName);
		}
	}

	if (!MediaOutput)
	{
		Transaction.Cancel();
	}
	
	return MediaOutput;
}

#undef LOCTEXT_NAMESPACE
