// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaMediaDevice.h"
#include "AvaOutputTreeItem.h"
#include "MediaIOCoreDefinitions.h"

class FAvaOutputClassItem;

class FAvaOutputDeviceItem : public FAvaOutputTreeItem
{
public:
	UE_AVA_INHERITS_WITH_SUPER(FAvaOutputDeviceItem, FAvaOutputTreeItem);

	FAvaOutputDeviceItem(const TSharedPtr<FAvaOutputTreeItem>& InParent, const FAvaMediaDevice& InDevice)
		: Super(InParent)
		, Device(InDevice)
	{
	}

	const FAvaMediaDevice& GetDevice() const
	{
		return Device;
	}

	//~ Begin IAvaOutputTreeItem
	virtual FText GetDisplayName() const override;
	virtual const FSlateBrush* GetIconBrush() const override;
	virtual void RefreshChildren() override;
	virtual TSharedPtr<SWidget> GenerateRowWidget() override;
	virtual bool IsValidToDropInChannel(FName InTargetChannelName) override;
	virtual UMediaOutput* AddMediaOutputToChannel(FName InTargetChannel, const FAvaMediaOutputInfo& InOutputInfo) override;
	//~ End IAvaOutputTreeItem

protected:
	FAvaMediaDevice Device;
};
