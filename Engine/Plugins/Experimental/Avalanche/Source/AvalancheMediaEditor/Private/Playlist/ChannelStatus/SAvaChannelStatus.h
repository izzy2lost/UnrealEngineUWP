// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Channel/AvaOutputChannel.h"
#include "Widgets/SCompoundWidget.h"

class SImage;
class UAvalancheBroadcast;

/*
 * Widget displaying the Status of a Single Channel
 */
class SAvaChannelStatus : public SCompoundWidget
{
public:
	
	SLATE_BEGIN_ARGS(SAvaChannelStatus) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const FAvaOutputChannel& InChannel);

	virtual ~SAvaChannelStatus() override;
	
	void OnChannelChanged(const FAvaOutputChannel& InChannel, EAvaChannelChange InChange);
	
protected:

	FName ChannelName;

	TSharedPtr<SImage> ChannelStatusIcon;
};
