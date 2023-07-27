// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/OutlinerColumns/SMuteColumnWidget.h"

#include "MVVM/MuteEditorExtension.h"
#include "MVVM/ViewModels/SequencerEditorViewModel.h"

namespace UE::Sequencer
{

void SMuteColumnWidget::OnToggleOperationComplete()
{
	// refresh the sequencer tree after operation is complete
	RefreshSequencerTree();
}

void SMuteColumnWidget::Construct(const FArguments& InArgs, const TWeakPtr<ISequencerOutlinerColumn> InWeakOutlinerColumn, const UE::Sequencer::FCreateOutlinerColumnParams& InParams)
{
	SColumnToggleWidget::Construct(
		SColumnToggleWidget::FArguments(),
		InWeakOutlinerColumn,
		InParams
	);
}

bool SMuteColumnWidget::IsActive() const
{
	TSharedPtr<FSequencerEditorViewModel> SequencerEditor = WeakEditor.Pin();
	if (SequencerEditor)
	{
		FMuteEditorExtension* MuteEditorExtension = SequencerEditor->CastDynamic<FMuteEditorExtension>();
		if (MuteEditorExtension)
		{
			return MuteEditorExtension->IsNodeMuted(WeakOutlinerExtension);
		}
	}

	return false;
}

void SMuteColumnWidget::SetIsActive(const bool bInIsActive)
{
	TSharedPtr<FSequencerEditorViewModel> SequencerEditor = WeakEditor.Pin();
	if (SequencerEditor)
	{
		FMuteEditorExtension* MuteEditorExtension = SequencerEditor->CastDynamic<FMuteEditorExtension>();
		if (MuteEditorExtension)
		{
			MuteEditorExtension->SetNodeMuted(WeakOutlinerExtension, bInIsActive);
		}
	}
}

bool SMuteColumnWidget::IsChildActive() const
{
	TSharedPtr<FSequencerEditorViewModel> SequencerEditor = WeakEditor.Pin();
	if (SequencerEditor)
	{
		FMuteEditorExtension* MuteEditorExtension = SequencerEditor->CastDynamic<FMuteEditorExtension>();
		if (MuteEditorExtension)
		{
			return MuteEditorExtension->HasMutedChildNode(WeakOutlinerExtension);
		}
	}

	return false;
}

bool SMuteColumnWidget::IsImplicitlyActive() const
{
	TSharedPtr<FSequencerEditorViewModel> SequencerEditor = WeakEditor.Pin();
	if (SequencerEditor)
	{
		FMuteEditorExtension* MuteEditorExtension = SequencerEditor->CastDynamic<FMuteEditorExtension>();
		if (MuteEditorExtension)
		{
			return MuteEditorExtension->IsNodeImplicitlyMuted(WeakOutlinerExtension);
		}
	}

	return false;
}

const FSlateBrush* SMuteColumnWidget::GetActiveBrush() const
{
	static const FName NAME_MutedBrush = TEXT("Sequencer.Column.Mute");
	return FAppStyle::Get().GetBrush(NAME_MutedBrush);
}

} // namespace UE::Sequencer
