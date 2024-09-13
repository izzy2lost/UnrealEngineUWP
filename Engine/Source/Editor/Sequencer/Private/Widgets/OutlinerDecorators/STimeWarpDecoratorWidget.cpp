// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/OutlinerDecorators/STimeWarpDecoratorWidget.h"

#include "MVVM/SharedViewModelData.h"
#include "MVVM/ViewModels/ViewModelIterators.h"
#include "MVVM/ViewModels/EditorViewModel.h"
#include "MVVM/ViewModels/OutlinerColumns/IOutlinerColumn.h"
#include "MVVM/ViewModels/OutlinerDecorators/IOutlinerDecorator.h"
#include "MVVM/ViewModels/OutlinerDecorators/TimeWarpOutlinerDecorator.h"
#include "MVVM/Selection/SequencerCoreSelection.h"
#include "MVVM/Selection/SequencerOutlinerSelection.h"

namespace UE::Sequencer
{

void STimeWarpDecoratorWidget::Construct(const FArguments& InArgs, const TWeakPtr<IOutlinerColumn> InWeakOutlinerColumn, const TWeakPtr<FTimeWarpOutlinerDecorator>& InWeakOutlinerDecorator, const FCreateOutlinerColumnParams& InParams)
{
	SColumnToggleWidget::Construct(
		SColumnToggleWidget::FArguments(),
		InWeakOutlinerColumn,
		InParams
	);

	WeakOutlinerDecorator = InWeakOutlinerDecorator;
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
	FLinearColor OutColor = FLinearColor::Black;
	float Opacity = 1.0f;
	////OutColor.A = Opacity; // The background should be at the specified opacity and the icon should stay 100% black.

	if (WeakOutlinerDecorator.IsValid())
	{
		WeakOutlinerDecorator.Pin()->Opacity = Opacity;
	}

	return OutColor;
}

} // namespace UE::Sequencer
