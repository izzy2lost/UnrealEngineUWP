// Copyright Epic Games, Inc. All Rights Reserved.

#include "SAvaPageChannelSelector.h"
#include "AvalancheBroadcast.h"
#include "Playlist/Pages/PageViews/AvaInstancedPageView.h"
#include "Playlist/Pages/PageViews/AvaInstancedPageViewImpl.h"
#include "Playlist/Pages/PageViews/AvaPageViewImpl.h"
#include "Playlist/Pages/Slate/SAvaPageViewRow.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"

void SAvaPageChannelSelector::Construct(const FArguments& InArgs, const FAvaPageViewRef& InPageView, const TSharedPtr<SAvaPageViewRow>& InRow)
{
	UpdateChannelNames();

	const TSharedRef<IAvaInstancedPageView> InstancedPageView = StaticCastSharedRef<IAvaInstancedPageView>(
		StaticCastSharedRef<FAvaInstancedPageViewImpl>(
			StaticCastSharedRef<FAvaPageViewImpl>(InPageView)
		)
	);
	
	PageViewWeak = InstancedPageView;
	PageViewRowWeak = InRow;
	
	ChildSlot
	[
		SAssignNew(ChannelCombo, SComboBox<FName>)
		.InitiallySelectedItem(InstancedPageView->GetChannelName())
		.OptionsSource(&ChannelNames)
		.OnGenerateWidget(this, &SAvaPageChannelSelector::GenerateChannelWidget)
		.OnSelectionChanged(this, &SAvaPageChannelSelector::OnChannelSelectionChanged)
		.OnComboBoxOpening(this, &SAvaPageChannelSelector::OnComboBoxOpening)
		[
			SNew(STextBlock)
			.Text(this, &SAvaPageChannelSelector::GetCurrentChannelName)
		]
	];
}

TSharedRef<SWidget> SAvaPageChannelSelector::GenerateChannelWidget(FName InChannelName)
{
	return SNew(STextBlock)
		.Text(FText::FromName(InChannelName));
}

void SAvaPageChannelSelector::OnChannelSelectionChanged(SComboBox<FName>::NullableOptionType InProposedSelection, ESelectInfo::Type InSelectInfo)
{
	if (const FAvaInstancedPageViewPtr PageView = PageViewWeak.Pin())
	{
		PageView->SetChannel(InProposedSelection);
	}
}

FText SAvaPageChannelSelector::GetCurrentChannelName() const
{
	if (const FAvaInstancedPageViewPtr PageView = PageViewWeak.Pin())
	{
		return FText::FromName(PageView->GetChannelName());
	}
	return FText();
}

void SAvaPageChannelSelector::OnComboBoxOpening()
{
	UpdateChannelNames();
	
	const FAvaInstancedPageViewPtr PageView = PageViewWeak.Pin();

	if (!PageView.IsValid())
	{
		return;
	}
	
	check(ChannelCombo.IsValid());
	ChannelCombo->SetSelectedItem(PageView->GetChannelName());
}

void SAvaPageChannelSelector::UpdateChannelNames()
{
	const UAvalancheBroadcast& AvalancheBroadcast = UAvalancheBroadcast::Get();
	ChannelNames.Reset(AvalancheBroadcast.GetChannelNameCount());
	
	for (int32 ChannelIndex = 0; ChannelIndex < AvalancheBroadcast.GetChannelNameCount(); ++ChannelIndex)
	{
		const FName ChannelName = AvalancheBroadcast.GetChannelName(ChannelIndex); 
		if (AvalancheBroadcast.GetChannelType(ChannelName) == EAvaBroadcastChannelType::Program)
		{
			ChannelNames.Add(ChannelName);
		}
	}
}
