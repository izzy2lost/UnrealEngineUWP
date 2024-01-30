// Copyright Epic Games, Inc. All Rights Reserved.

#include "SAvaChannelStatusList.h"
#include "AvalancheBroadcast.h"
#include "SAvaChannelStatus.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Layout/SWrapBox.h"

void SAvaChannelStatusList::Construct(const FArguments& InArgs)
{
	BroadcastWeak = UAvalancheBroadcast::GetAvalancheBroadcast();
	
	check(BroadcastWeak.IsValid());
	
	BroadcastWeak->AddChangeListener(FOnAvaBroadcastChanged::FDelegate::CreateSP(this
		, &SAvaChannelStatusList::OnBroadcastChanged));
	
	ChildSlot
	[
		SAssignNew(WrapBox, SWrapBox)
		.UseAllottedSize(true)
		.HAlign(EHorizontalAlignment::HAlign_Center)
	];

	RefreshList();
}

SAvaChannelStatusList::~SAvaChannelStatusList()
{
	if (BroadcastWeak.IsValid())
	{
		BroadcastWeak->RemoveChangeListener(this);
	}
}

void SAvaChannelStatusList::RefreshList()
{
	UAvalancheBroadcast* const Broadcast = BroadcastWeak.Get();
	
	if (!Broadcast || !WrapBox.IsValid())
	{
		return;
	}

	WrapBox->ClearChildren();

	const TArray<FAvaOutputChannel*>& Channels = Broadcast->GetCurrentProfile().GetChannels();
	
	for (const FAvaOutputChannel* Channel : Channels)
	{
		WrapBox->AddSlot()
			[
				SNew(SAvaChannelStatus, *Channel)
			];
	}
}

void SAvaChannelStatusList::OnBroadcastChanged(EAvaBroadcastChange InChange)
{
	if (EnumHasAnyFlags(InChange, EAvaBroadcastChange::CurrentProfile
		| EAvaBroadcastChange::ChannelGrid
		| EAvaBroadcastChange::ChannelRename))
	{
		RefreshList();
	}
}
