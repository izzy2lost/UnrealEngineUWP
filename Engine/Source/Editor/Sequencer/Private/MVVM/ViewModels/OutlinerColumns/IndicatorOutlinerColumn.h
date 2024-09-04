// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MVVM/ViewModels/OutlinerColumns/OutlinerColumnBase.h"
#include "Styling/SlateColor.h"

namespace UE::Sequencer
{

	class SConditionColumnWidget;

	/**
	 * A column for showing various indicators on rows based on the presence of features (e.g. conditions, time warp) on that row.
	 */
	class FIndicatorOutlinerColumn
		: public FOutlinerColumnBase
	{
	public:

		FIndicatorOutlinerColumn();

		bool IsItemCompatibleWithColumn(const FCreateOutlinerColumnParams& InParams) const override;
		TSharedPtr<SWidget> CreateColumnWidget(const FCreateOutlinerColumnParams& InParams, const TSharedRef<ISequencerTreeViewRow>& TreeViewRow) override;

	private:
		FLinearColor GetColumnBackgroundColorAndOpacity() const;

	private:
		TSharedPtr<SConditionColumnWidget> ColumnWidget;
	};

} // namespace UE::Sequencer