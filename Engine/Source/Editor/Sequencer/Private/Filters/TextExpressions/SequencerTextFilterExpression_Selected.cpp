// Copyright Epic Games, Inc. All Rights Reserved.

#include "SequencerTextFilterExpression_Selected.h"
#include "EditorModeManager.h"
#include "Filters/Filters/SequencerTrackFilter_Selected.h"
#include "Selection.h"
#include "Sequencer.h"

using namespace UE::Sequencer;

#define LOCTEXT_NAMESPACE "SequencerTextFilterExpression_Selected"

FSequencerTextFilterExpression_Selected::FSequencerTextFilterExpression_Selected(ISequencerTrackFilters& InFilterInterface)
	: FSequencerTextFilterExpressionContext(InFilterInterface)
{
	if (const TSharedPtr<IToolkitHost> ToolkitHost = FilterInterface.GetSequencer().GetToolkitHost())
	{
		ToolkitHost->GetEditorModeManager().GetSelectedObjects()->SelectionChangedEvent.AddRaw(this, &FSequencerTextFilterExpression_Selected::OnSelectionChanged);
	}
}

FSequencerTextFilterExpression_Selected::~FSequencerTextFilterExpression_Selected()
{
	if (const TSharedPtr<IToolkitHost> ToolkitHost = FilterInterface.GetSequencer().GetToolkitHost())
	{
		ToolkitHost->GetEditorModeManager().GetSelectedObjects()->SelectionChangedEvent.RemoveAll(this);
	}
}

TSet<FName> FSequencerTextFilterExpression_Selected::GetKeys() const
{
	return { TEXT("SELECTED"), TEXT("VIEWPORT") };
}

ESequencerTextFilterValueType FSequencerTextFilterExpression_Selected::GetValueType() const
{
	return ESequencerTextFilterValueType::Boolean;
}

FText FSequencerTextFilterExpression_Selected::GetDescription() const
{
	return LOCTEXT("ExpressionDescription_Selected", "Filter by viewport selection state");
}

bool FSequencerTextFilterExpression_Selected::TestComplexExpression(const FName& InKey
	, const FTextFilterString& InValue
	, const ETextFilterComparisonOperation InComparisonOperation
	, const ETextFilterTextComparisonMode InTextComparisonMode) const
{
	if (!FSequencerTextFilterExpressionContext::TestComplexExpression(InKey, InValue, InComparisonOperation, InTextComparisonMode))
	{
		return true;
	}

	const TSharedPtr<FSequencerTrackFilter_Selected> Filter = MakeShared<FSequencerTrackFilter_Selected>(FilterInterface);
	const bool bFilterPassed = Filter->PassesFilter(FilterItem);
	return CompareFStringForExactBool(InValue, InComparisonOperation, bFilterPassed);
}

void FSequencerTextFilterExpression_Selected::OnSelectionChanged(UObject* const InObject)
{
	//FilterBar.RequestFilterUpdate();
}

#undef LOCTEXT_NAMESPACE
