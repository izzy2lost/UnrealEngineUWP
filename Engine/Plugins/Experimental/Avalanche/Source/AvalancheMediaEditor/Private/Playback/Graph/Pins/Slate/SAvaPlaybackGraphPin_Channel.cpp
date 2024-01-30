// Copyright Epic Games, Inc. All Rights Reserved.

#include "SAvaPlaybackGraphPin_Channel.h"

#include "AvaMediaEditorStyle.h"
#include "AvaMediaEditorUtils.h"
#include "AvalancheBroadcast.h"
#include "Playback/Graph/AvaPlaybackGraphSchema.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/SBoxPanel.h"

void SAvaPlaybackGraphPin_Channel::Construct(const FArguments& InArgs, UEdGraphPin* InPin)
{
	SGraphPin::Construct(SGraphPin::FArguments()
			.UsePinColorForText(true)
		, InPin);
	
	UpdateChannelState();
}

const FAvaOutputChannel* SAvaPlaybackGraphPin_Channel::GetChannel() const
{
	if (const UEdGraphPin* const ChannelPin = GetPinObj())
	{
		const FAvaOutputChannel& Channel = UAvalancheBroadcast::Get().GetCurrentProfile().GetChannel(ChannelPin->GetFName());
		if (Channel.IsValidChannel())
		{
			return &Channel;
		}
	}
	return nullptr;
}

void SAvaPlaybackGraphPin_Channel::UpdateChannelState()
{
	bPinEnabled = false;
	
	if (const FAvaOutputChannel* const Channel = GetChannel())
	{
		bPinEnabled = true;
		ChannelStateText = FAvaMediaEditorUtils::GetChannelStatusText(Channel->GetState(), Channel->GetIssueSeverity());
		ChannelStateBrush = FAvaMediaEditorUtils::GetChannelStatusBrush(Channel->GetState(), Channel->GetIssueSeverity());
	}
}

TSharedRef<SWidget> SAvaPlaybackGraphPin_Channel::GetDefaultValueWidget()
{
	return SNew(SHorizontalBox)
		+SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SImage)
			.Image(this, &SAvaPlaybackGraphPin_Channel::GetChannelStatusBrush)
		]
		+SHorizontalBox::Slot()
		.Padding(2.f, 0.f, 0.f, 0.f)
		.HAlign(EHorizontalAlignment::HAlign_Left)
		.VAlign(EVerticalAlignment::VAlign_Center)
		.AutoWidth()
		[
			SNew(STextBlock)
			.Text(this, &SAvaPlaybackGraphPin_Channel::GetChannelStatusText)
		];
}

FText SAvaPlaybackGraphPin_Channel::GetChannelStatusText() const
{
	return ChannelStateText;
}

const FSlateBrush* SAvaPlaybackGraphPin_Channel::GetChannelStatusBrush() const
{
	return ChannelStateBrush;
}

FSlateColor SAvaPlaybackGraphPin_Channel::GetPinColor() const
{
	return bPinEnabled
		? UAvaPlaybackGraphSchema::ActivePinColor
		: UAvaPlaybackGraphSchema::InactivePinColor;
}	
