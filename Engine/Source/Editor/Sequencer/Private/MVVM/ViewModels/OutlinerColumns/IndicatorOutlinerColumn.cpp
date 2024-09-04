// Copyright Epic Games, Inc. All Rights Reserved.

#include "MVVM/ViewModels/OutlinerColumns/IndicatorOutlinerColumn.h"
#include "Widgets/OutlinerColumns/SConditionColumnWidget.h"
#include "MVVM/Extensions/IConditionableExtension.h"
#include "MVVM/SharedViewModelData.h"
#include "Widgets/Layout/SBorder.h"

#define LOCTEXT_NAMESPACE "FIndicatorOutlinerColumn"

namespace UE::Sequencer
{
	FIndicatorOutlinerColumn::FIndicatorOutlinerColumn()
	{
		Name = FCommonOutlinerNames::Indicator;
		Label = LOCTEXT("IndicatorColumnLabel", "Indicators");
		Position = FOutlinerColumnPosition{ 0, EOutlinerColumnGroup::FarLeftGutter };
		Layout = FOutlinerColumnLayout{ 14, FMargin(4.f, 0.f), HAlign_Center, VAlign_Center, EOutlinerColumnSizeMode::Fixed, EOutlinerColumnFlags::None };
	}

	bool FIndicatorOutlinerColumn::IsItemCompatibleWithColumn(const FCreateOutlinerColumnParams& InParams) const
	{
		// TODO: For now only show condition info, but eventually this should show all indicators
		if (FConditionStateCacheExtension* ConditionStateCache = InParams.OutlinerExtension.AsModel()->GetSharedData()->CastThis<FConditionStateCacheExtension>())
		{
			return EnumHasAnyFlags(ConditionStateCache->GetCachedFlags(InParams.OutlinerExtension), ECachedConditionState::HasCondition | ECachedConditionState::ParentHasCondition | ECachedConditionState::ChildHasCondition | ECachedConditionState::SectionHasCondition);
		}

		return false;
	}

	TSharedPtr<SWidget> FIndicatorOutlinerColumn::CreateColumnWidget(const FCreateOutlinerColumnParams& InParams, const TSharedRef<ISequencerTreeViewRow>& TreeViewRow)
	{
		//// TODO: For now only show condition info, but eventually this should show all indicators- probably wrap various widgets in one outer widget.
		
		// Currently not working attempt at getting a background color behind the column widget 
		//return SNew(SOverlay)
		//	+ SOverlay::Slot()
		//	.HAlign(HAlign_Fill)
		//	.VAlign(VAlign_Fill)
		//	[
		//		SNew(SBorder)
		//		.BorderImage(FAppStyle::GetBrush("Menu.Background"))
		//		.ColorAndOpacity(this, &FIndicatorOutlinerColumn::GetColumnBackgroundColorAndOpacity)
		//		.HAlign(HAlign_Fill)
		//		.VAlign(VAlign_Fill)
		//	]
		//	+ SOverlay::Slot()
		//	[
		//		SAssignNew(ColumnWidget, SConditionColumnWidget, SharedThis(this), InParams)
		//	];

		return SAssignNew(ColumnWidget, SConditionColumnWidget, SharedThis(this), InParams);

	}

	FLinearColor FIndicatorOutlinerColumn::GetColumnBackgroundColorAndOpacity() const
	{
		static const FLinearColor ConditionBGColor(92.0f, 220.0f, 205.0f);
		
		FLinearColor BGColor = ConditionBGColor;

		if (ColumnWidget.IsValid())
		{
			FSlateColor WidgetColor = ColumnWidget->GetImageColorAndOpacity();
			BGColor.A = WidgetColor.GetSpecifiedColor().A;
		}
		return BGColor;
	}

} // namespace UE::Sequencer

#undef LOCTEXT_NAMESPACE