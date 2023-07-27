// Copyright Epic Games, Inc. All Rights Reserved.

#include "OutlinerColumns/PinOutlinerColumn.h"

#include "MVVM/Extensions/IOutlinerExtension.h"
#include "ISequencerOutlinerColumn.h"
#include "MVVM/ViewModels/EditorViewModel.h"
#include "MVVM/PinEditorExtension.h"
#include "MVVM/ViewModels/OutlinerItemModel.h"
#include "MVVM/ViewModels/SequencerEditorViewModel.h"
#include "Widgets/OutlinerColumns/SPinColumnWidget.h"
#include "Widgets/Layout/SScaleBox.h"

#define LOCTEXT_NAMESPACE "FPinOutlinerColumn"

FName FPinOutlinerColumn::GetColumnName() const
{
	return FName(TEXT("Pin"));
}

FText FPinOutlinerColumn::GetColumnLabel() const
{
	return LOCTEXT("PinColumnLabel", "Pin");
}

bool FPinOutlinerColumn::IsItemCompatibleWithColumn(const UE::Sequencer::FCreateOutlinerColumnParams& InParams) const
{
	using namespace UE::Sequencer;

	if (InParams.Editor)
	{
		FPinEditorExtension* PinEditorExtension = InParams.Editor->CastDynamic<FPinEditorExtension>();
		if (PinEditorExtension)
		{
			return PinEditorExtension->IsNodePinnable(InParams.OutlinerExtension);
		}
	}

	return false;
}

TSharedRef<ISequencerOutlinerColumn> FPinOutlinerColumn::CreateOutlinerColumn()
{
	return MakeShareable(new FPinOutlinerColumn());
}

TSharedRef<SWidget> FPinOutlinerColumn::CreateColumnWidget(const TWeakPtr<ISequencerOutlinerColumn> InWeakOutlinerColumn, const UE::Sequencer::FCreateOutlinerColumnParams& InParams) const
{
	using namespace UE::Sequencer;

	return SNew(SPinColumnWidget,
		InWeakOutlinerColumn,
		InParams);
}

#undef LOCTEXT_NAMESPACE