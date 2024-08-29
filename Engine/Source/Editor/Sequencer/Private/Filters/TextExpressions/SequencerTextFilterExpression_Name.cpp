// Copyright Epic Games, Inc. All Rights Reserved.

#include "SequencerTextFilterExpression_Name.h"
#include "MVVM/ViewModelPtr.h"
#include "MVVM/Extensions/IOutlinerExtension.h"

using namespace UE::Sequencer;

#define LOCTEXT_NAMESPACE "SequencerTextFilterExpression_Name"

FSequencerTextFilterExpression_Name::FSequencerTextFilterExpression_Name(ISequencerTrackFilters& InFilterInterface)
	: FSequencerTextFilterExpressionContext(InFilterInterface)
{
}

TSet<FName> FSequencerTextFilterExpression_Name::GetKeys() const
{
	return { TEXT("NAME") };
}

ESequencerTextFilterValueType FSequencerTextFilterExpression_Name::GetValueType() const
{
	return ESequencerTextFilterValueType::String;
}

FText FSequencerTextFilterExpression_Name::GetDescription() const
{
	return LOCTEXT("ExpressionDescription_Name", "Filter by track name");
}

bool FSequencerTextFilterExpression_Name::TestComplexExpression(const FName& InKey
	, const FTextFilterString& InValue
	, const ETextFilterComparisonOperation InComparisonOperation
	, const ETextFilterTextComparisonMode InTextComparisonMode) const
{
	if (!FSequencerTextFilterExpressionContext::TestComplexExpression(InKey, InValue, InComparisonOperation, InTextComparisonMode))
	{
		return true;
	}

	if (WeakTrackObject.IsValid()
		&& !TextFilterUtils::TestComplexExpression(WeakTrackObject->GetName(), InValue, InComparisonOperation, InTextComparisonMode))
	{
		return false;
	}

	if (const TViewModelPtr<IOutlinerExtension> OutlinerExtension = FilterItem.ImplicitCast())
	{
		if (!TextFilterUtils::TestComplexExpression(OutlinerExtension->GetLabel().ToString(), InValue, InComparisonOperation, InTextComparisonMode))
		{
			return false;
		}
	}

	return true;
}

#undef LOCTEXT_NAMESPACE
