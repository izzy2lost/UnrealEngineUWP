// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaRundownFilterIdExpressionFactory.h"

#include "Misc/TextFilterUtils.h"
#include "Rundown/AvaRundownPage.h"

const FName FAvaRundownFilterIdExpressionFactory::KeyName = FName(TEXT("ID"));

FName FAvaRundownFilterIdExpressionFactory::GetFilterIdentifier() const
{
	return KeyName;
}

bool FAvaRundownFilterIdExpressionFactory::FilterExpression(const FAvaRundownPage& InItem, const FAvaRundownTextFilterArgs& InArgs) const
{
	const FTextFilterString FilterString = FString::FromInt(InItem.GetPageId());
	if (FilterString.CanCompareNumeric(InArgs.ValueToCheck))
	{
		return FilterString.CompareNumeric(InArgs.ValueToCheck, InArgs.ComparisonOperation);
	}
	return false;
}

bool FAvaRundownFilterIdExpressionFactory::SupportsComparisonOperation(ETextFilterComparisonOperation InComparisonOperation, EAvaRundownSearchListType InRundownSearchListType) const
{
	return true;
}
