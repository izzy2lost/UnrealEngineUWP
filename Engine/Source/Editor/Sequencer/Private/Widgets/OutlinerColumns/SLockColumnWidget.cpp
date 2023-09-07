// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/OutlinerColumns/SLockColumnWidget.h"
#include "MVVM/SharedViewModelData.h"
#include "MVVM/ViewModels/SequencerEditorViewModel.h"
#include "MVVM/Extensions/ILockableExtension.h"
#include "MVVM/ViewModels/SequenceModel.h"
#include "MVVM/Selection/Selection.h"

namespace UE::Sequencer
{

void SLockColumnWidget::Construct(const FArguments& InArgs, const TWeakPtr<ISequencerOutlinerColumn> InWeakOutlinerColumn, const UE::Sequencer::FCreateOutlinerColumnParams& InParams)
{
	SColumnToggleWidget::Construct(
		SColumnToggleWidget::FArguments(),
		InWeakOutlinerColumn,
		InParams
	);

	WeakLockStateCacheExtension = CastViewModel<FLockStateCacheExtension>(InParams.OutlinerExtension.AsModel()->GetSharedData());
}

bool SLockColumnWidget::IsActive() const
{
	if (TViewModelPtr<FLockStateCacheExtension> StateCache = WeakLockStateCacheExtension.Pin())
	{
		return EnumHasAnyFlags(StateCache->GetCachedFlags(ModelID), ECachedLockState::Locked);
	}

	return false;
}

void SLockColumnWidget::SetIsActive(const bool bInIsActive)
{
	TViewModelPtr<IOutlinerExtension> OutlinerItem = WeakOutlinerExtension.Pin();
	if (!OutlinerItem)
	{
		return;
	}

	TSharedPtr<FSequenceModel> SequenceModel = OutlinerItem.AsModel()->FindAncestorOfType<FSequenceModel>();
	if (!SequenceModel)
	{
		return;
	}

	const FScopedTransaction Transaction(NSLOCTEXT("Sequencer", "SetNodeLocked", "Set Node Locked"));

	if (OutlinerItem->GetSelectionState() == EOutlinerSelectionState::SelectedDirectly)
	{
		// modify all selected items
		for (FViewModelPtr OutlinerNode : SequenceModel->GetEditor()->GetSelection()->Outliner)
		{
			for (TSharedPtr<ILockableExtension> Lockable : OutlinerNode->GetDescendantsOfType<ILockableExtension>(true))
			{
				Lockable->SetIsLocked(bInIsActive);
			}
		}
	}
	else
	{
		// only one unselected item was toggled, toggle just that node
		FViewModelPtr Item = OutlinerItem;
		for (TSharedPtr<ILockableExtension> Lockable : Item->GetDescendantsOfType<ILockableExtension>(true))
		{
			Lockable->SetIsLocked(bInIsActive);
		}
	}
}

bool SLockColumnWidget::IsChildActive() const
{
	if (TViewModelPtr<FLockStateCacheExtension> StateCache = WeakLockStateCacheExtension.Pin())
	{
		return EnumHasAnyFlags(StateCache->GetCachedFlags(ModelID), ECachedLockState::PartiallyLockedChildren);
	}

	return false;
}

const FSlateBrush* SLockColumnWidget::GetActiveBrush() const
{
	static const FName NAME_LockBrush = TEXT("Sequencer.Column.Locked");
	return FAppStyle::Get().GetBrush(NAME_LockBrush);
}

} // namespace UE::Sequencer
