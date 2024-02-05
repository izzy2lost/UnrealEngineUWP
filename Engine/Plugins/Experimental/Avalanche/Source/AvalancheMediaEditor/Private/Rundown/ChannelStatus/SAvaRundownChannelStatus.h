// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Broadcast/Channel/AvaBroadcastOutputChannel.h"
#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class SImage;
class UAvaBroadcast;

/*
 * Widget displaying the Status of a Single Channel
 */
class SAvaRundownChannelStatus : public SCompoundWidget
{
public:
	
	SLATE_BEGIN_ARGS(SAvaRundownChannelStatus) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const FAvaBroadcastOutputChannel& InChannel);

	virtual ~SAvaRundownChannelStatus() override;
	
	void OnChannelChanged(const FAvaBroadcastOutputChannel& InChannel, EAvaBroadcastChannelChange InChange);
	
protected:

	FName ChannelName;

	TSharedPtr<SImage> ChannelStatusIcon;
};
