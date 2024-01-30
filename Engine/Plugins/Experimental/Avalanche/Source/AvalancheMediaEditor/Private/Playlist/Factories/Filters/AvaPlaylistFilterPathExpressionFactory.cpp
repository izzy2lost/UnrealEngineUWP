// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaPlaylistFilterPathExpressionFactory.h"
#include "Misc/TextFilterUtils.h"
#include "Playlist/AvalanchePage.h"

const FName FAvaPlaylistFilterPathExpressionFactory::KeyName = FName(TEXT("ASSET"));

FName FAvaPlaylistFilterPathExpressionFactory::GetFilterIdentifier() const
{
	return KeyName;
}

bool FAvaPlaylistFilterPathExpressionFactory::FilterExpression(const FAvalanchePage& InItem, const FAvaPlaylistTextFilterArgs& InArgs) const
{
	if (!InArgs.ItemPlaylist)
	{
		return false;
	}
	const FName AssetPath = FName(InItem.GetAvalancheAssetPath(InArgs.ItemPlaylist).GetAssetPathString());
	return InArgs.ValueToCheck.CompareName(AssetPath, InArgs.ComparisonMode);
}

bool FAvaPlaylistFilterPathExpressionFactory::SupportsComparisonOperation(ETextFilterComparisonOperation InComparisonOperation, EAvaPlaylistSearchListType InPlaylistSearchListType) const
{
	return InComparisonOperation == ETextFilterComparisonOperation::Equal || InComparisonOperation == ETextFilterComparisonOperation::NotEqual;
}
