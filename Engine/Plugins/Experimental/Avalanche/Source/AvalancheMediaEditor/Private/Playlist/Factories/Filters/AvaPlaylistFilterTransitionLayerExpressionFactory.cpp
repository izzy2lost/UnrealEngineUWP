// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaPlaylistFilterTransitionLayerExpressionFactory.h"
#include "Misc/TextFilterUtils.h"
#include "Playlist/AvalanchePage.h"

const FName FAvaPlaylistFilterTransitionLayerExpressionFactory::KeyName = FName(TEXT("TRANSITIONLAYER"));

FName FAvaPlaylistFilterTransitionLayerExpressionFactory::GetFilterIdentifier() const
{
	return KeyName;
}

bool FAvaPlaylistFilterTransitionLayerExpressionFactory::FilterExpression(const FAvalanchePage& InItem, const FAvaPlaylistTextFilterArgs& InArgs) const
{
	if (!InArgs.ItemPlaylist)
	{
		return false;
	}
	return InArgs.ValueToCheck.CompareFString(InItem.GetTransitionLayer(InArgs.ItemPlaylist).ToString(), InArgs.ComparisonMode);
}

bool FAvaPlaylistFilterTransitionLayerExpressionFactory::SupportsComparisonOperation(ETextFilterComparisonOperation InComparisonOperation, EAvaPlaylistSearchListType InPlaylistSearchListType) const
{
	return InComparisonOperation == ETextFilterComparisonOperation::Equal || InComparisonOperation == ETextFilterComparisonOperation::NotEqual;
}
