// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaRundownFilterTransitionLayerExpressionFactory.h"

#include "Misc/TextFilterUtils.h"
#include "Rundown/AvaRundownPage.h"

const FName FAvaRundownFilterTransitionLayerExpressionFactory::KeyName = FName(TEXT("TRANSITIONLAYER"));

FName FAvaRundownFilterTransitionLayerExpressionFactory::GetFilterIdentifier() const
{
	return KeyName;
}

bool FAvaRundownFilterTransitionLayerExpressionFactory::FilterExpression(const FAvaRundownPage& InItem, const FAvaRundownTextFilterArgs& InArgs) const
{
	if (!InArgs.ItemRundown)
	{
		return false;
	}
	return InArgs.ValueToCheck.CompareFString(InItem.GetTransitionLayer(InArgs.ItemRundown).ToString(), InArgs.ComparisonMode);
}

bool FAvaRundownFilterTransitionLayerExpressionFactory::SupportsComparisonOperation(ETextFilterComparisonOperation InComparisonOperation, EAvaRundownSearchListType InRundownSearchListType) const
{
	return InComparisonOperation == ETextFilterComparisonOperation::Equal || InComparisonOperation == ETextFilterComparisonOperation::NotEqual;
}
