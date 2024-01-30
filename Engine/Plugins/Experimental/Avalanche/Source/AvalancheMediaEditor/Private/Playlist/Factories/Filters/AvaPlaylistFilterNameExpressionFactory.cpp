// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaPlaylistFilterNameExpressionFactory.h"
#include "Misc/TextFilterUtils.h"
#include "Playlist/AvalanchePage.h"

const FName FAvaPlaylistFilterNameExpressionFactory::KeyName = FName(TEXT("NAME"));

FName FAvaPlaylistFilterNameExpressionFactory::GetFilterIdentifier() const
{
	return KeyName;
}

bool FAvaPlaylistFilterNameExpressionFactory::FilterExpression(const FAvalanchePage& InItem, const FAvaPlaylistTextFilterArgs& InArgs) const
{
	const FName Name = FName(InItem.GetPageDescription().ToString());
	return InArgs.ValueToCheck.CompareName(Name, InArgs.ComparisonMode);
}

bool FAvaPlaylistFilterNameExpressionFactory::SupportsComparisonOperation(ETextFilterComparisonOperation InComparisonOperation, EAvaPlaylistSearchListType InPlaylistSearchListType) const
{
	return InComparisonOperation == ETextFilterComparisonOperation::Equal || InComparisonOperation == ETextFilterComparisonOperation::NotEqual;
}
