// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Playlist/AvaPlaylistDefines.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/SCompoundWidget.h"

class IAvaInstancedPageView;
class SAvaPageViewRow;

class SAvaPageChannelSelector : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SAvaPageChannelSelector){}
	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs, const FAvaPageViewRef& InPageView, const TSharedPtr<SAvaPageViewRow>& InRow);

	TSharedRef<SWidget> GenerateChannelWidget(FName InChannelName);
	
	void OnChannelSelectionChanged(SComboBox<FName>::NullableOptionType InProposedSelection, ESelectInfo::Type InSelectInfo);
	
	FText GetCurrentChannelName() const;
	
	void OnComboBoxOpening();
	
protected:
	void UpdateChannelNames();

	TWeakPtr<IAvaInstancedPageView> PageViewWeak;
	
	TWeakPtr<SAvaPageViewRow> PageViewRowWeak;
	
	TSharedPtr<SComboBox<FName>> ChannelCombo;

	TArray<FName> ChannelNames;

	bool bInEditingMode = false;
};
