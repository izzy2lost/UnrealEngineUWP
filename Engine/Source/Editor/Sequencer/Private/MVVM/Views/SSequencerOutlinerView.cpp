// Copyright Epic Games, Inc. All Rights Reserved.

#include "MVVM/Views/SSequencerOutlinerView.h"
#include "MVVM/Extensions/IOutlinerExtension.h"
#include "MVVM/ViewModels/OutlinerViewModel.h"
#include "MVVM/Views/SOutlinerItemViewBase.h"

#include "Styling/StyleColors.h"

namespace UE::Sequencer
{

class SSequencerOutlinerViewRow : public SOutlinerViewRow
{
public:

	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& OwnerTableView, TWeakViewModelPtr<IOutlinerExtension> InWeakModel, TWeakPtr<FOutlinerViewModel> InWeakOutliner)
	{
		WeakOutliner = InWeakOutliner;

		SOutlinerViewRow::Construct(InArgs, OwnerTableView, InWeakModel);

		if (InWeakModel.Pin()->HasBackground())
		{
			SetBorderImage(TAttribute<const FSlateBrush*>(FAppStyle::GetBrush("WhiteBrush")));
			SetBorderBackgroundColor(MakeAttributeSP(this, &SSequencerOutlinerViewRow::GetBorderTint));
		}
	}

	FSlateColor GetBorderTint() const
	{
		TSharedPtr<FOutlinerViewModel>    Outliner     = WeakOutliner.Pin();
		TViewModelPtr<IOutlinerExtension> OutlinerItem = WeakModel.Pin();

		if (!Outliner || !OutlinerItem)
		{
			return FLinearColor(0.f,0.f,0.f,0.f);
		}

		EOutlinerSelectionState SelectionState = OutlinerItem->GetSelectionState();

		if (EnumHasAnyFlags(SelectionState, EOutlinerSelectionState::SelectedDirectly))
		{
			return FStyleColors::Select;
		}
		if (EnumHasAnyFlags(SelectionState, EOutlinerSelectionState::HasSelectedKeys | EOutlinerSelectionState::HasSelectedTrackAreaItems))
		{
			return FStyleColors::Header;
		}

		// If this is collapsed but has any children with selected keys or sections, we report that state on the parent
		if (!OutlinerItem->IsExpanded())
		{
			if (EnumHasAnyFlags(SelectionState, EOutlinerSelectionState::DescendentHasSelectedTrackAreaItems | EOutlinerSelectionState::DescendentHasSelectedKeys))
			{
				return FStyleColors::Header;
			}
		}

		// @todo: Currently we are always using 'default' for the style here. A lot of this is carried over from earlier Sequencer UIs
		//        so we either need to re-evaluate whether EOutlinerItemViewBaseStyle is even necessary, or add proper support to the outliner extension to specify it
		EOutlinerItemViewBaseStyle ItemStyle = EOutlinerItemViewBaseStyle::Default; /* OutlinerItem->GetItemViewStyle() */;

		if (Outliner->GetHoveredItem() == OutlinerItem)
		{
			return /*ItemStyle == EOutlinerItemViewBaseStyle::ContainerHeader
				? FLinearColor(FColor(52, 52, 52, 255))
				: */FLinearColor(FColor(72, 72, 72, 255));
		}

		return /*ItemStyle == EOutlinerItemViewBaseStyle::ContainerHeader
			? FLinearColor(FColor(48, 48, 48, 255))
			: */FLinearColor(FColor(62, 62, 62, 255));
	}

private:

	TWeakPtr<FOutlinerViewModel> WeakOutliner;
};

TSharedRef<ITableRow> SSequencerOutlinerView::OnGenerateRow(TWeakViewModelPtr<IOutlinerExtension> InWeakModel, const TSharedRef<STableViewBase>& OwnerTable)
{
	TSharedRef<SOutlinerViewRow> Row =
		SNew(SSequencerOutlinerViewRow, OwnerTable, InWeakModel, WeakOutliner)
		.OnDetectDrag(this, &SSequencerOutlinerView::OnDragRow)
		.OnGenerateWidgetForColumn(this, &SSequencerOutlinerView::GenerateWidgetForColumn);

	if (TViewModelPtr<IOutlinerExtension> ViewModel = InWeakModel.Pin())
	{
		CreateTrackLanesForRow(Row, ViewModel);
	}

	return Row;
}


} // namespace UE::Sequencer