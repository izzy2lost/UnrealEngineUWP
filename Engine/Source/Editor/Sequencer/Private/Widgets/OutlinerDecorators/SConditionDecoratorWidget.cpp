// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/OutlinerDecorators/SConditionDecoratorWidget.h"

#include "MVVM/SharedViewModelData.h"
#include "MVVM/ViewModels/ViewModelIterators.h"
#include "MVVM/ViewModels/EditorViewModel.h"
#include "MVVM/ViewModels/OutlinerColumns/IOutlinerColumn.h"
#include "MVVM/ViewModels/OutlinerDecorators/IOutlinerDecorator.h"
#include "MVVM/ViewModels/OutlinerDecorators/ConditionOutlinerDecorator.h"
#include "MVVM/Extensions/IConditionableExtension.h"
#include "MVVM/Selection/SequencerCoreSelection.h"
#include "MVVM/Selection/SequencerOutlinerSelection.h"

namespace UE::Sequencer
{

void SConditionDecoratorWidget::Construct(const FArguments& InArgs, const TWeakPtr<IOutlinerColumn> InWeakOutlinerColumn, const TWeakPtr<FConditionOutlinerDecorator>& InWeakOutlinerDecorator, const FCreateOutlinerColumnParams& InParams)
{
	SColumnToggleWidget::Construct(
		SColumnToggleWidget::FArguments(),
		InWeakOutlinerColumn,
		InParams
	);

	WeakConditionStateCacheExtension = CastViewModel<FConditionStateCacheExtension>(InParams.OutlinerExtension.AsModel()->GetSharedData());

	WeakOutlinerDecorator = InWeakOutlinerDecorator;
}

bool SConditionDecoratorWidget::IsActive() const
{
	if (TViewModelPtr<FConditionStateCacheExtension> StateCache = WeakConditionStateCacheExtension.Pin())
	{
		return EnumHasAnyFlags(StateCache->GetCachedFlags(ModelID), ECachedConditionState::HasCondition);
	}

	return false;
}

const FSlateBrush* SConditionDecoratorWidget::GetActiveBrush() const
{
	static const FName NAME_ConditionBrush = TEXT("Sequencer.Decorator.Condition");
	return FAppStyle::Get().GetBrush(NAME_ConditionBrush);
}

FSlateColor SConditionDecoratorWidget::GetImageColorAndOpacity() const
{
	TSharedPtr<FEditorViewModel> Editor = WeakEditor.Pin();
	TViewModelPtr<IOutlinerExtension> OutlinerItem = WeakOutlinerExtension.Pin();

	bool bActive = IsActive();
	bool bEvaluatingTrue = false;
	bool bChildHasCondition = false;
	bool bSectionHasCondition = false;
	if (TViewModelPtr<FConditionStateCacheExtension> StateCache = WeakConditionStateCacheExtension.Pin())
	{
		bEvaluatingTrue = EnumHasAnyFlags(StateCache->GetCachedFlags(ModelID), ECachedConditionState::ConditionEvaluatingTrue);
		bChildHasCondition = EnumHasAnyFlags(StateCache->GetCachedFlags(ModelID), ECachedConditionState::ChildHasCondition);
		bSectionHasCondition = EnumHasAnyFlags(StateCache->GetCachedFlags(ModelID), ECachedConditionState::SectionHasCondition);
	}
	FLinearColor OutColor = FLinearColor::Black;

	if (!Editor || !OutlinerItem)
	{
		return OutColor;
	}

	float Opacity = 1.0f;

	if (IsActive())
	{
		if (bEvaluatingTrue)
		{
			Opacity = 1.0f;
		}
		else
		{
			Opacity = 0.5f;
		}
	}
	else if ((bChildHasCondition && !OutlinerItem->IsExpanded()) || bSectionHasCondition)
	{
		Opacity = 0.25f;
	}
	else
	{
		// Not active, invisible
		Opacity = 0.0f;
	}
	////OutColor.A = Opacity; // The background should be at the specified opacity and the icon should stay 100% black.

	if (WeakOutlinerDecorator.IsValid())
	{
		WeakOutlinerDecorator.Pin()->Opacity = Opacity;
	}

	return OutColor;
}

} // namespace UE::Sequencer
