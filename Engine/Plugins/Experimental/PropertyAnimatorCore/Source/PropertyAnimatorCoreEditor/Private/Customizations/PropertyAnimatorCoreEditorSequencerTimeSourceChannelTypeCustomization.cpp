// Copyright Epic Games, Inc. All Rights Reserved.

#include "Customizations/PropertyAnimatorCoreEditorSequencerTimeSourceChannelTypeCustomization.h"

#include "DetailWidgetRow.h"
#include "Editor.h"
#include "Selection.h"
#include "Sequencer/MovieSceneAnimatorTrackEditor.h"
#include "TimeSources/PropertyAnimatorCoreSequencerTimeSource.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "PropertyAnimatorCoreEditorSequencerTimeSourceChannelTypeCustomization"

void FPropertyAnimatorCoreEditorSequencerTimeSourceChannelTypeCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> InPropertyHandle, FDetailWidgetRow& InRow, IPropertyTypeCustomizationUtils& InUtils)
{
	if (!InPropertyHandle->IsValidHandle())
	{
		return;
	}

	ChannelPropertyHandle = InPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FPropertyAnimatorCoreSequencerTimeSourceChannel, Channel));

	if (!ChannelPropertyHandle)
	{
		return;
	}

	InRow.NameContent()
	[
		InPropertyHandle->CreatePropertyNameWidget()
	];

	InRow.ValueContent()
	.HAlign(HAlign_Fill)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.FillWidth(1.f)
		[
			ChannelPropertyHandle->CreatePropertyValueWidget()
		]
		+ SHorizontalBox::Slot()
		.Padding(5.f, 0.f)
		.AutoWidth()
		[
			SNew(SButton)
			.HAlign(HAlign_Fill)
			.Visibility(this, &FPropertyAnimatorCoreEditorSequencerTimeSourceChannelTypeCustomization::GetCreateTrackButtonVisibility)
			.OnClicked(this, &FPropertyAnimatorCoreEditorSequencerTimeSourceChannelTypeCustomization::OnCreateTrackButtonClicked)
			.IsEnabled(this, &FPropertyAnimatorCoreEditorSequencerTimeSourceChannelTypeCustomization::IsCreateTrackButtonEnabled)
			[
				SNew(STextBlock)
				.Font(IPropertyTypeCustomizationUtils::GetRegularFont())
				.Text(LOCTEXT("AddSequencerTrack", "Add Sequencer track"))
			]
		]
	];
}

void FPropertyAnimatorCoreEditorSequencerTimeSourceChannelTypeCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> InPropertyHandle, IDetailChildrenBuilder& InBuilder, IPropertyTypeCustomizationUtils& InUtils)
{
}

FReply FPropertyAnimatorCoreEditorSequencerTimeSourceChannelTypeCustomization::OnCreateTrackButtonClicked()
{
	TArray<AActor*> SelectedActors;

	if (GEditor)
	{
		if (USelection* ActorSelection = GEditor->GetSelectedActors())
		{
			ActorSelection->GetSelectedObjects<AActor>(SelectedActors);
		}
	}

	if (ChannelPropertyHandle.IsValid())
	{
		uint8 Channel = 0;

		if (ChannelPropertyHandle->GetValue(Channel) == FPropertyAccess::Success)
		{
			FMovieSceneAnimatorTrackEditor::OnAddAnimatorTrack.Broadcast(Channel);

			// Reselect actors after track was created
			if (GEditor)
			{
				GEditor->SelectNone(/** Notify */false, /** DeselectBSP */true);

				for (int32 Index = 0; Index < SelectedActors.Num(); Index++)
				{
					GEditor->SelectActor(SelectedActors[Index], /** Selected */true, /** Notify */Index == SelectedActors.Num() - 1);
				}
			}
		}
	}

	return FReply::Handled();
}

EVisibility FPropertyAnimatorCoreEditorSequencerTimeSourceChannelTypeCustomization::GetCreateTrackButtonVisibility() const
{
	return ChannelPropertyHandle.IsValid()
		&& ChannelPropertyHandle->GetNumPerObjectValues() == 1
		? EVisibility::Visible
		: EVisibility::Collapsed;
}

bool FPropertyAnimatorCoreEditorSequencerTimeSourceChannelTypeCustomization::IsCreateTrackButtonEnabled() const
{
	if (!ChannelPropertyHandle.IsValid() || ChannelPropertyHandle->GetNumPerObjectValues() != 1)
	{
		return false;
	}

	uint8 Channel = 0;
	int32 Count = 0;

	if (ChannelPropertyHandle->GetValue(Channel) == FPropertyAccess::Success)
	{
		FMovieSceneAnimatorTrackEditor::OnGetAnimatorTrackCount.Broadcast(Channel, Count);
	}

	return Count == 0;
}

#undef LOCTEXT_NAMESPACE
