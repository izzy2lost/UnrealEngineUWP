// Copyright Epic Games, Inc. All Rights Reserved.

#include "SAvaPreviewChannelSelector.h"
#include "AvalancheBroadcast.h"
#include "AvalancheMediaSettings.h"

void SAvaPreviewChannelSelector::Construct(const FArguments& InArgs)
{
	UpdateChannelNames();
	
	ChildSlot
	[
		SAssignNew(ChannelCombo, SComboBox<FName>)
		.InitiallySelectedItem(GetPreviewChannelNameFromSettings())
		.OptionsSource(&ChannelNames)
		.OnGenerateWidget(this, &SAvaPreviewChannelSelector::GenerateChannelWidget)
		.OnSelectionChanged(this, &SAvaPreviewChannelSelector::OnChannelSelectionChanged)
		.OnComboBoxOpening(this, &SAvaPreviewChannelSelector::OnComboBoxOpening)
		[
			SNew(STextBlock)
			.Text(this, &SAvaPreviewChannelSelector::GetCurrentChannelName)
		]
	];
}

TSharedRef<SWidget> SAvaPreviewChannelSelector::GenerateChannelWidget(FName InChannelName)
{
	return SNew(STextBlock)
		.Text(FText::FromName(InChannelName));
}

void SAvaPreviewChannelSelector::OnChannelSelectionChanged(SComboBox<FName>::NullableOptionType InProposedSelection, ESelectInfo::Type InSelectInfo)
{
	UAvalancheMediaSettings& AvalancheMediaSettings = UAvalancheMediaSettings::GetMutable();
	AvalancheMediaSettings.PreviewChannelName = !InProposedSelection.IsNone() ? InProposedSelection.ToString() : FString();
	AvalancheMediaSettings.SaveConfig();
}

FText SAvaPreviewChannelSelector::GetCurrentChannelName() const
{
	return FText::FromName(GetPreviewChannelNameFromSettings());
}

FName SAvaPreviewChannelSelector::GetPreviewChannelNameFromSettings()
{
	const UAvalancheMediaSettings& Settings = UAvalancheMediaSettings::Get();	
	return !Settings.PreviewChannelName.IsEmpty() ? FName(Settings.PreviewChannelName) : NAME_None; 
}

void SAvaPreviewChannelSelector::OnComboBoxOpening()
{
	UpdateChannelNames();
	
	check(ChannelCombo.IsValid());
	ChannelCombo->SetSelectedItem(GetPreviewChannelNameFromSettings());
}

void SAvaPreviewChannelSelector::UpdateChannelNames()
{
	const UAvalancheBroadcast& AvaBroadcast = UAvalancheBroadcast::Get();
	ChannelNames.Reset(AvaBroadcast.GetChannelNameCount() + 1);
	ChannelNames.Add(NAME_None);	// Add none to allow user to deselect.
	
	for (int32 ChannelIndex = 0; ChannelIndex < AvaBroadcast.GetChannelNameCount(); ++ChannelIndex)
	{
		const FName ChannelName = AvaBroadcast.GetChannelName(ChannelIndex);
		if (AvaBroadcast.GetChannelType(ChannelName) == EAvaBroadcastChannelType::Preview)
		{
			ChannelNames.Add(ChannelName);
		}
	}
}
