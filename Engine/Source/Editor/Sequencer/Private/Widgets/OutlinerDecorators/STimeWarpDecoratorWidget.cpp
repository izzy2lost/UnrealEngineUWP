// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/OutlinerDecorators/STimeWarpDecoratorWidget.h"

#include "MVVM/SharedViewModelData.h"
#include "MVVM/ViewModels/ViewModelIterators.h"
#include "MVVM/ViewModels/EditorViewModel.h"
#include "MVVM/ViewModels/OutlinerColumns/IOutlinerColumn.h"
#include "MVVM/ViewModels/OutlinerDecorators/IOutlinerDecoratorBuilder.h"
#include "MVVM/ViewModels/OutlinerDecorators/TimeWarpOutlinerDecoratorBuilder.h"
#include "MVVM/Selection/SequencerCoreSelection.h"
#include "MVVM/Selection/SequencerOutlinerSelection.h"

namespace UE::Sequencer
{

void STimeWarpDecoratorWidget::Construct(const FArguments& InArgs, const TWeakPtr<IOutlinerColumn> InWeakOutlinerColumn, const FCreateOutlinerColumnParams& InParams)
{
	SColumnToggleWidget::Construct(
		SColumnToggleWidget::FArguments(),
		InWeakOutlinerColumn,
		InParams
	);
}

bool STimeWarpDecoratorWidget::IsActive() const
{
	return true;
}

const FSlateBrush* STimeWarpDecoratorWidget::GetActiveBrush() const
{
	static const FName NAME_TimeWarpBrush = TEXT("Sequencer.Decorator.TimeWarp");
	return FAppStyle::Get().GetBrush(NAME_TimeWarpBrush);
}

FSlateColor STimeWarpDecoratorWidget::GetImageColorAndOpacity() const
{
	return FLinearColor::Black;
}

} // namespace UE::Sequencer
