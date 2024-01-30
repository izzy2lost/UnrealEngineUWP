// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaOutputTreeItem.h"
#include "OutputDevices/AvaDeviceProviderData.h"
#include "UObject/WeakObjectPtr.h"
#include "UObject/WeakObjectPtrTemplates.h"

class FAvaOutputServerItem : public FAvaOutputTreeItem
{
public:
	UE_AVA_INHERITS_WITH_SUPER(FAvaOutputServerItem, FAvaOutputTreeItem);

	FAvaOutputServerItem(const FString& InServerName, const TSharedPtr<const FAvaDeviceProviderDataList>& InDeviceProviderDataList)
		: Super(nullptr)
		, ServerName(InServerName)
		, DeviceProviderDataList(InDeviceProviderDataList)
	{
	}

	FString GetServerName() const;
	const FAvaDeviceProviderData* GetDeviceProviderData(FName InDeviceProviderName) const;

	//~ Begin IAvaOutputTreeItem
	virtual FText GetDisplayName() const override;
	virtual const FSlateBrush* GetIconBrush() const override;
	virtual void RefreshChildren() override;
	virtual TSharedPtr<SWidget> GenerateRowWidget() override;
	virtual bool IsValidToDropInChannel(FName InTargetChannelName) override { return false; }
	virtual UMediaOutput* AddMediaOutputToChannel(FName InTargetChannel, const FAvaMediaOutputInfo& InOutputInfo) override;
	//~ End IAvaOutputTreeItem

protected:
	FString ServerName;
	const TSharedPtr<const FAvaDeviceProviderDataList> DeviceProviderDataList;
};
