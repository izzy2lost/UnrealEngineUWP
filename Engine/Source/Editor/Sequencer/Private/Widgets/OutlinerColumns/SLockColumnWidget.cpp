// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/OutlinerColumns/SLockColumnWidget.h"

#include "MVVM/LockEditorExtension.h"
#include "MVVM/ViewModels/SequencerEditorViewModel.h"

namespace UE::Sequencer
{

void SLockColumnWidget::Construct(const FArguments& InArgs, const TWeakPtr<ISequencerOutlinerColumn> InWeakOutlinerColumn, const UE::Sequencer::FCreateOutlinerColumnParams& InParams)
{
	SColumnToggleWidget::Construct(
		SColumnToggleWidget::FArguments(),
		InWeakOutlinerColumn,
		InParams
	);
}

bool SLockColumnWidget::IsActive() const
{
	TSharedPtr<FSequencerEditorViewModel> SequencerEditor = WeakEditor.Pin();
	if (SequencerEditor)
	{
		FLockEditorExtension* LockEditorExtension = SequencerEditor->CastDynamic<FLockEditorExtension>();
		if (LockEditorExtension)
		{
			return LockEditorExtension->IsNodeLocked(WeakOutlinerExtension);
		}
	}

	return false;
}

void SLockColumnWidget::SetIsActive(const bool bInIsActive)
{
	TSharedPtr<FSequencerEditorViewModel> SequencerEditor = WeakEditor.Pin();
	if (SequencerEditor)
	{
		FLockEditorExtension* LockEditorExtension = SequencerEditor->CastDynamic<FLockEditorExtension>();
		if (LockEditorExtension)
		{
			LockEditorExtension->SetNodeLocked(WeakOutlinerExtension, bInIsActive);
		}
	}
}

bool SLockColumnWidget::IsChildActive() const
{
	TSharedPtr<FSequencerEditorViewModel> SequencerEditor = WeakEditor.Pin();
	if (SequencerEditor)
	{
		FLockEditorExtension* LockEditorExtension = SequencerEditor->CastDynamic<FLockEditorExtension>();
		if (LockEditorExtension)
		{
			return LockEditorExtension->HasLockedChildNode(WeakOutlinerExtension);
		}
	}

	return false;
}

const FSlateBrush* SLockColumnWidget::GetActiveBrush() const
{
	static const FName NAME_LockBrush = TEXT("Sequencer.Column.Locked");
	return FAppStyle::Get().GetBrush(NAME_LockBrush);
}

} // namespace UE::Sequencer
