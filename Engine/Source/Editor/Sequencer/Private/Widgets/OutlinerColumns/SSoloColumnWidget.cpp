// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/OutlinerColumns/SSoloColumnWidget.h"

#include "MVVM/SharedViewModelData.h"
#include "MVVM/Selection/Selection.h"
#include "MVVM/Extensions/ISoloableExtension.h"
#include "MVVM/ViewModels/SequenceModel.h"
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

	WeakSoloStateCacheExtension = CastViewModel<FSoloStateCacheExtension>(InParams.OutlinerExtension.AsModel()->GetSharedData());
}

bool SSoloColumnWidget::IsActive() const
{
	if (TViewModelPtr<FSoloStateCacheExtension> SoloStateCache = WeakSoloStateCacheExtension.Pin())
	{
		return EnumHasAnyFlags(SoloStateCache->GetCachedFlags(ModelID), ECachedSoloState::Soloed);
	}

	return false;
}

void SSoloColumnWidget::SetIsActive(const bool bInIsActive)
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

	const FScopedTransaction Transaction(NSLOCTEXT("Sequencer", "SetNodeMuted", "Set Node Soloed"));

	if (OutlinerItem->GetSelectionState() == EOutlinerSelectionState::SelectedDirectly)
	{
		// if selected, modify all selected items
		for (TViewModelPtr<ISoloableExtension> Soloable : SequenceModel->GetEditor()->GetSelection()->Outliner.Filter<ISoloableExtension>())
		{
			Soloable->SetIsSoloed(bInIsActive);
		}
	}
	else if (TViewModelPtr<ISoloableExtension> Soloable = OutlinerItem.ImplicitCast())
	{
		Soloable->SetIsSoloed(bInIsActive);
	}
}

bool SSoloColumnWidget::IsChildActive() const
{
	if (TViewModelPtr<FSoloStateCacheExtension> SoloStateCache = WeakSoloStateCacheExtension.Pin())
	{
		return EnumHasAnyFlags(SoloStateCache->GetCachedFlags(ModelID), ECachedSoloState::PartiallySoloedChildren);
	}

	return false;
}

bool SSoloColumnWidget::IsImplicitlyActive() const
{
	if (TViewModelPtr<FSoloStateCacheExtension> SoloStateCache = WeakSoloStateCacheExtension.Pin())
	{
		return EnumHasAnyFlags(SoloStateCache->GetCachedFlags(ModelID), ECachedSoloState::ImplicitlySoloedByParent);
	}

	return false;
}

const FSlateBrush* SSoloColumnWidget::GetActiveBrush() const
{
	static const FName NAME_SoloedBrush = TEXT("Sequencer.Column.Solo");
	return FAppStyle::Get().GetBrush(NAME_SoloedBrush);
}

} // namespace UE::Sequencer
