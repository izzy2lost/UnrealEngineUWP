// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaMediaDefines.h"
#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class SAvaRundownChannelStatus;
class SWrapBox;
class UAvaBroadcast;

/*
 * Widget containing the Status of all Channels in Broadcast. It contains a list of SAvaRundownChannelStatus widgets
 */
class SAvaRundownChannelStatusList : public SCompoundWidget
{
public:
	
	SLATE_BEGIN_ARGS(SAvaRundownChannelStatusList){}
	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs);
	
	virtual ~SAvaRundownChannelStatusList() override;

	void RefreshList();
	
	void OnBroadcastChanged(EAvaBroadcastChange InChange);

protected:

	TSharedPtr<SWrapBox> WrapBox;

	TWeakObjectPtr<UAvaBroadcast> BroadcastWeak;

	TMap<FName, TSharedPtr<SAvaRundownChannelStatus>> ChannelStatusSlots;
};
