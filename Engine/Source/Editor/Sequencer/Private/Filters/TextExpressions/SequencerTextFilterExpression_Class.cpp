// Copyright Epic Games, Inc. All Rights Reserved.

#include "SequencerTextFilterExpression_Class.h"
#include "Filters/SequencerFilterData.h"
#include "Sequencer.h"

using namespace UE::Sequencer;

#define LOCTEXT_NAMESPACE "SequencerTextFilterExpression_Class"

FSequencerTextFilterExpression_Class::FSequencerTextFilterExpression_Class(ISequencerTrackFilters& InFilterInterface)
	: FSequencerTextFilterExpressionContext(InFilterInterface)
{
}

TSet<FName> FSequencerTextFilterExpression_Class::GetKeys() const
{
	return { TEXT("CLASS"), TEXT("TYPE") };
}

ESequencerTextFilterValueType FSequencerTextFilterExpression_Class::GetValueType() const
{
	return ESequencerTextFilterValueType::String;
}

FText FSequencerTextFilterExpression_Class::GetDescription() const
{
	return LOCTEXT("ExpressionDescription_Class", "Filter by class name");
}

bool FSequencerTextFilterExpression_Class::TestComplexExpression(const FName& InKey
	, const FTextFilterString& InValue
	, const ETextFilterComparisonOperation InComparisonOperation
	, const ETextFilterTextComparisonMode InTextComparisonMode) const
{
	if (!FSequencerTextFilterExpressionContext::TestComplexExpression(InKey, InValue, InComparisonOperation, InTextComparisonMode))
	{
		return true;
	}

	if (const TViewModelPtr<ITrackExtension> TrackExtension = FilterItem->FindAncestorOfType<ITrackExtension>())
	{
		for (const TViewModelPtr<FSectionModel>& SectionModel : TrackExtension->GetSectionModels().IterateSubList<FSectionModel>())
		{
			if (const auto SectionInterface = SectionModel->GetSectionInterface())
			{
				const FString SectionTitle = SectionInterface->GetSectionTitle().ToString();

				if (TextFilterUtils::TestComplexExpression(SectionTitle, InValue, InComparisonOperation, InTextComparisonMode))
				{
					return true;
				}
			}
		}
	}

	if (WeakTrackObject.IsValid())
	{
		const FString TrackClassName = WeakTrackObject->GetClass()->GetName();
		if (TextFilterUtils::TestComplexExpression(TrackClassName, InValue, InComparisonOperation, InTextComparisonMode))
		{
			return true;
		}
		return false;
	}

	ISequencer& Sequencer = FilterInterface.GetSequencer();

	UObject* const BoundObject = FSequencerTrackFilter::ResolveTrackBoundObject(Sequencer, FilterItem, FilterInterface.GetFilterData());
	if (IsValid(BoundObject)
		&& !TextFilterUtils::TestComplexExpression(BoundObject->GetClass()->GetName(), InValue, InComparisonOperation, InTextComparisonMode))
	{
		return false;
	}

	return true;
}

#undef LOCTEXT_NAMESPACE
