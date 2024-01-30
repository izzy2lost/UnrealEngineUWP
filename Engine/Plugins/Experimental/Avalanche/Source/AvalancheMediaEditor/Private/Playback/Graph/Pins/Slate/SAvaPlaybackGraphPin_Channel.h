// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SGraphPin.h"

struct FAvaOutputChannel;

class SAvaPlaybackGraphPin_Channel : public SGraphPin
{
public:

	SLATE_BEGIN_ARGS(SAvaPlaybackGraphPin_Channel){}
	SLATE_END_ARGS()
	
	void Construct(const FArguments& InArgs, UEdGraphPin* InPin);
	
	const FAvaOutputChannel* GetChannel() const;
	
	void UpdateChannelState();
	
	/** Build the widget we should put into the 'default value' space, shown when nothing connected */
	virtual TSharedRef<SWidget>	GetDefaultValueWidget() override;

	FText GetChannelStatusText() const;
	
	const FSlateBrush* GetChannelStatusBrush() const;

	virtual FSlateColor GetPinColor() const override;
	
protected:

	const FSlateBrush* ChannelStateBrush = nullptr;
	
	FText ChannelStateText;
	
	bool bPinEnabled = true;
};
