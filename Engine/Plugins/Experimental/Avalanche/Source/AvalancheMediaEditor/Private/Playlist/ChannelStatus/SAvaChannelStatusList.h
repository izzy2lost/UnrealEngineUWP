// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaMediaDefines.h"
#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class SAvaChannelStatus;
class SWrapBox;
class UAvalancheBroadcast;

/*
 * Widget containing the Status of all Channels in Broadcast. It contains a list of SAvaChannelStatus widgets
 */
class SAvaChannelStatusList : public SCompoundWidget
{
public:
	
	SLATE_BEGIN_ARGS(SAvaChannelStatusList){}
	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs);
	
	virtual ~SAvaChannelStatusList() override;

	void RefreshList();
	
	void OnBroadcastChanged(EAvaBroadcastChange InChange);

protected:

	TSharedPtr<SWrapBox> WrapBox;

	TWeakObjectPtr<UAvalancheBroadcast> BroadcastWeak;

	TMap<FName, TSharedPtr<SAvaChannelStatus>> ChannelStatusSlots;
};
