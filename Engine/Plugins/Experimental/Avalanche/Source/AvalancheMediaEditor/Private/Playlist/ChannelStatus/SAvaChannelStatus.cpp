// Copyright Epic Games, Inc. All Rights Reserved.

#include "SAvaChannelStatus.h"

#include "AvaMediaEditorStyle.h"
#include "AvaMediaEditorUtils.h"
#include "AvalancheBroadcast.h"
#include "Components/HorizontalBox.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"

void SAvaChannelStatus::Construct(const FArguments& InArgs, const FAvaOutputChannel& InChannel)
{
	ChannelName = InChannel.GetChannelName();
	FAvaOutputChannel::GetOnChannelChanged().AddSP(this, &SAvaChannelStatus::OnChannelChanged);

	ChildSlot
	.Padding(FMargin(10.f, 3.f))
	[
		SNew(SHorizontalBox)		
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SAssignNew(ChannelStatusIcon, SImage)
			.DesiredSizeOverride(FVector2D(16.f))
		]
		+ SHorizontalBox::Slot()
		.FillWidth(1.f)
		[
			SNew(STextBlock)
			.Text(FText::FromName(ChannelName))
		]
	];

	OnChannelChanged(InChannel, EAvaChannelChange::State);
}

SAvaChannelStatus::~SAvaChannelStatus()
{
	FAvaOutputChannel::GetOnChannelChanged().RemoveAll(this);
}

void SAvaChannelStatus::OnChannelChanged(const FAvaOutputChannel& InChannel, EAvaChannelChange InChange)
{
	const bool bMatchingChannels = InChannel.GetChannelName() == ChannelName;
	const bool bStateChanged     = EnumHasAnyFlags(InChange, EAvaChannelChange::State);
	
	if (!bMatchingChannels || !bStateChanged)
	{
		return;
	}
	
	ChannelStatusIcon->SetImage(FAvaMediaEditorUtils::GetChannelStatusBrush(InChannel.GetState(), InChannel.GetIssueSeverity()));
}
