// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaRundownFilterPathExpressionFactory.h"

#include "Misc/TextFilterUtils.h"
#include "Rundown/AvaRundownPage.h"

const FName FAvaRundownFilterPathExpressionFactory::KeyName = FName(TEXT("ASSET"));

FName FAvaRundownFilterPathExpressionFactory::GetFilterIdentifier() const
{
	return KeyName;
}

bool FAvaRundownFilterPathExpressionFactory::FilterExpression(const FAvaRundownPage& InItem, const FAvaRundownTextFilterArgs& InArgs) const
{
	if (!InArgs.ItemRundown)
	{
		return false;
	}
	const FName AssetPath = FName(InItem.GetAssetPath(InArgs.ItemRundown).GetAssetPathString());
	return InArgs.ValueToCheck.CompareName(AssetPath, InArgs.ComparisonMode);
}

bool FAvaRundownFilterPathExpressionFactory::SupportsComparisonOperation(ETextFilterComparisonOperation InComparisonOperation, EAvaRundownSearchListType InRundownSearchListType) const
{
	return InComparisonOperation == ETextFilterComparisonOperation::Equal || InComparisonOperation == ETextFilterComparisonOperation::NotEqual;
}
