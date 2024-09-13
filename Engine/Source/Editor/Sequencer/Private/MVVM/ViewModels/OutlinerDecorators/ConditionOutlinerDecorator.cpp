// Copyright Epic Games, Inc. All Rights Reserved.

#include "MVVM/ViewModels/OutlinerDecorators/ConditionOutlinerDecorator.h"

#include "MVVM/ViewModels/OutlinerColumns/IOutlinerColumn.h"
#include "MVVM/ViewModels/OutlinerColumns/OutlinerColumnTypes.h"
#include "Widgets/OutlinerDecorators/SConditionDecoratorWidget.h"
#include "MVVM/Extensions/IConditionableExtension.h"
#include "MVVM/SharedViewModelData.h"
#include "Widgets/Layout/SBorder.h"

#define LOCTEXT_NAMESPACE "FConditionOutlinerDecorator"

namespace UE::Sequencer
{

FConditionOutlinerDecorator::FConditionOutlinerDecorator()
{
	Opacity = 1.f;
}

FName FConditionOutlinerDecorator::GetDecoratorName() const
{
	return FCommonOutlinerNames::Condition;
}

bool FConditionOutlinerDecorator::IsItemCompatibleWithDecorator(const FCreateOutlinerColumnParams& InParams) const
{
	if (FConditionStateCacheExtension* ConditionStateCache = InParams.OutlinerExtension.AsModel()->GetSharedData()->CastThis<FConditionStateCacheExtension>())
	{
		return EnumHasAnyFlags(ConditionStateCache->GetCachedFlags(InParams.OutlinerExtension), ECachedConditionState::HasCondition | ECachedConditionState::ParentHasCondition | ECachedConditionState::ChildHasCondition | ECachedConditionState::SectionHasCondition);
	}

	return false;
}

TSharedPtr<SWidget> FConditionOutlinerDecorator::CreateDecoratorWidget(const FCreateOutlinerColumnParams& InParams, const TSharedRef<ISequencerTreeViewRow>& TreeViewRow, const TSharedRef<IOutlinerColumn>& OutlinerColumn, const int32 NumCompatibleDecorators)
{
	static const FLinearColor ConditionColor = FLinearColor::FromSRGBColor(FColor(92, 220, 205));

	if (NumCompatibleDecorators > 1)
	{
		return SNew(SBorder)
			.VAlign(VAlign_Fill)
			.HAlign(HAlign_Fill)
			.Padding(0.0f)
			.BorderImage(FAppStyle::GetBrush("WhiteBrush"))
			.BorderBackgroundColor_Lambda([this]() -> FLinearColor { FLinearColor BackgroundColor = ConditionColor; BackgroundColor.A = Opacity; return BackgroundColor; });
	}

	return SNew(SOverlay)
		+ SOverlay::Slot()
		[
			SNew(SBorder)
				.VAlign(VAlign_Fill)
				.HAlign(HAlign_Fill)
				.Padding(0.0f)
				.BorderImage(FAppStyle::GetBrush("WhiteBrush"))
				.BorderBackgroundColor_Lambda([this]() -> FLinearColor { FLinearColor BackgroundColor = ConditionColor; BackgroundColor.A = Opacity; return BackgroundColor; })
		]

		+ SOverlay::Slot()
		[
			SNew(SBox)
				.WidthOverride(12.f)
				.HeightOverride(12.f)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					SAssignNew(DecoratorWidget, SConditionDecoratorWidget, OutlinerColumn, SharedThis(this), InParams)
				]
		];
}

} // namespace UE::Sequencer

#undef LOCTEXT_NAMESPACE