// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/OutlinerColumns/SSoloColumnWidget.h"

#include "MVVM/SoloEditorExtension.h"
#include "MVVM/ViewModels/EditorViewModel.h"
#include "MVVM/ViewModels/SequencerEditorViewModel.h"

namespace UE::Sequencer
{

void SSoloColumnWidget::OnToggleOperationComplete()
{
	// refresh the sequencer tree after operation is complete
	RefreshSequencerTree();
}

void SSoloColumnWidget::Construct(const FArguments& InArgs, const TWeakPtr<ISequencerOutlinerColumn> InWeakOutlinerColumn, const UE::Sequencer::FCreateOutlinerColumnParams& InParams)
{
	SColumnToggleWidget::Construct(
		SColumnToggleWidget::FArguments(),
		InWeakOutlinerColumn,
		InParams
	);
}

bool SSoloColumnWidget::IsActive() const
{
	TSharedPtr<FSequencerEditorViewModel> SequencerEditor = WeakEditor.Pin();
	if (SequencerEditor)
	{
		FSoloEditorExtension* SoloEditorExtension = SequencerEditor->CastDynamic<FSoloEditorExtension>();
		if (SoloEditorExtension)
		{
			return SoloEditorExtension->IsNodeSoloed(WeakOutlinerExtension);
		}
	}

	return false;
}

void SSoloColumnWidget::SetIsActive(const bool bInIsActive)
{
	TSharedPtr<FSequencerEditorViewModel> SequencerEditor = WeakEditor.Pin();
	if (SequencerEditor)
	{
		FSoloEditorExtension* SoloEditorExtension = SequencerEditor->CastDynamic<FSoloEditorExtension>();
		if (SoloEditorExtension)
		{
			SoloEditorExtension->SetNodeSoloed(WeakOutlinerExtension, bInIsActive);
		}
	}
}

bool SSoloColumnWidget::IsChildActive() const
{
	TSharedPtr<FSequencerEditorViewModel> SequencerEditor = WeakEditor.Pin();
	if (SequencerEditor)
	{
		FSoloEditorExtension* SoloEditorExtension = SequencerEditor->CastDynamic<FSoloEditorExtension>();
		if (SoloEditorExtension)
		{
			return SoloEditorExtension->HasSoloedChildNode(WeakOutlinerExtension);
		}
	}

	return false;
}

bool SSoloColumnWidget::IsImplicitlyActive() const
{
	TSharedPtr<FSequencerEditorViewModel> SequencerEditor = WeakEditor.Pin();
	if (SequencerEditor)
	{
		FSoloEditorExtension* SoloEditorExtension = SequencerEditor->CastDynamic<FSoloEditorExtension>();
		if (SoloEditorExtension)
		{
			return SoloEditorExtension->IsNodeImplicitlySoloed(WeakOutlinerExtension);
		}
	}

	return false;
}

const FSlateBrush* SSoloColumnWidget::GetActiveBrush() const
{
	static const FName NAME_SoloedBrush = TEXT("Sequencer.Column.Solo");
	return FAppStyle::Get().GetBrush(NAME_SoloedBrush);
}

} // namespace UE::Sequencer
