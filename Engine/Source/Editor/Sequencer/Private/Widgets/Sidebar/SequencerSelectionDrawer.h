// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Sidebar/ISidebarDrawerContent.h"
#include "Templates/SharedPointerFwd.h"

class FMenuBuilder;
class FName;
class FSequencer;
class FText;
class ISidebarChannelExtension;
class SVerticalBox;
class SWidget;
struct FCurveChannelSectionSidebarExtension;
struct FKeyEditData;

namespace UE::Sequencer
{
	class FSequencerSelection;
}

class FSequencerSelectionDrawer : public ISidebarDrawerContent
{
public:
	static const FName UniqueId;

	FSequencerSelectionDrawer(const TWeakPtr<FSequencer>& InWeakSequencer);

	//~ Begin ISidebarDrawerContent
	virtual FName GetUniqueId() const override;
	virtual FName GetSectionId() const override;
	virtual FText GetSectionDisplayText() const override;
	virtual TSharedRef<SWidget> CreateContentWidget() override;
	//~ End ISidebarDrawerContent

protected:
	void OnSequencerSelectionChanged();
	void UpdateFromSelectionNextFrame();

	void BuildKeySelectionDetails(const TSharedRef<UE::Sequencer::FSequencerSelection>& InSelection, FMenuBuilder& MenuBuilder);
	void BuildTrackAreaDetails(const TSharedRef<UE::Sequencer::FSequencerSelection>& InSelection, FMenuBuilder& MenuBuilder);
	void BuildOutlinerDetails(const TSharedRef<UE::Sequencer::FSequencerSelection>& InSelection, FMenuBuilder& MenuBuilder);
	void BuildMarkedFrameDetails(const TSharedRef<UE::Sequencer::FSequencerSelection>& InSelection, FMenuBuilder& MenuBuilder);

	TSharedRef<SWidget> CreateHintText(const FText& InMessage);
	TSharedRef<SWidget> CreateNoSelectionHintText();

	FKeyEditData GetKeyEditData() const;
	
	TSharedPtr<SWidget> CreateKeyFrameDetails(const TSharedRef<UE::Sequencer::FSequencerSelection>& InSequencerSelection);
	TSharedPtr<SWidget> CreateMarkedFrameDetails(const int32 InMarkedFrameIndex);

	TWeakPtr<FSequencer> WeakSequencer;

	TSharedPtr<SVerticalBox> ContentBox;

	TSharedPtr<FCurveChannelSectionSidebarExtension> CurveChannelExtension;

	TArray<TSharedPtr<ISidebarChannelExtension>> ChannelExtensions;

	bool bWaitingToHandleSelectionChanged = false;
};
