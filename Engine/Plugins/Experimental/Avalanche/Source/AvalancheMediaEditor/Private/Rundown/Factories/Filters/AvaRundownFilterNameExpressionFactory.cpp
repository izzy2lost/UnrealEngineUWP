// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaRundownFilterNameExpressionFactory.h"

#include "Misc/TextFilterUtils.h"
#include "Rundown/AvaRundownPage.h"

const FName FAvaRundownFilterNameExpressionFactory::KeyName = FName(TEXT("NAME"));

FName FAvaRundownFilterNameExpressionFactory::GetFilterIdentifier() const
{
	return KeyName;
}

bool FAvaRundownFilterNameExpressionFactory::FilterExpression(const FAvaRundownPage& InItem, const FAvaRundownTextFilterArgs& InArgs) const
{
	const FName Name = FName(InItem.GetPageDescription().ToString());
	return InArgs.ValueToCheck.CompareName(Name, InArgs.ComparisonMode);
}

bool FAvaRundownFilterNameExpressionFactory::SupportsComparisonOperation(ETextFilterComparisonOperation InComparisonOperation, EAvaRundownSearchListType InRundownSearchListType) const
{
	return InComparisonOperation == ETextFilterComparisonOperation::Equal || InComparisonOperation == ETextFilterComparisonOperation::NotEqual;
}
