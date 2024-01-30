// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaPlaylistFilterIdExpressionFactory.h"
#include "Misc/TextFilterUtils.h"
#include "Playlist/AvalanchePage.h"
#include "Playlist/AvaPlaylistDefines.h"

const FName FAvaPlaylistFilterIdExpressionFactory::KeyName = FName(TEXT("ID"));

FName FAvaPlaylistFilterIdExpressionFactory::GetFilterIdentifier() const
{
	return KeyName;
}

bool FAvaPlaylistFilterIdExpressionFactory::FilterExpression(const FAvalanchePage& InItem, const FAvaPlaylistTextFilterArgs& InArgs) const
{
	const FTextFilterString FilterString = FString::FromInt(InItem.GetPageId());
	if (FilterString.CanCompareNumeric(InArgs.ValueToCheck))
	{
		return FilterString.CompareNumeric(InArgs.ValueToCheck, InArgs.ComparisonOperation);
	}
	return false;
}

bool FAvaPlaylistFilterIdExpressionFactory::SupportsComparisonOperation(ETextFilterComparisonOperation InComparisonOperation, EAvaPlaylistSearchListType InPlaylistSearchListType) const
{
	return true;
}
