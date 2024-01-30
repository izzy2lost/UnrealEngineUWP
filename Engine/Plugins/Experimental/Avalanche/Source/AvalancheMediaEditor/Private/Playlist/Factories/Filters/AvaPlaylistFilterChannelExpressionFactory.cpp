// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaPlaylistFilterChannelExpressionFactory.h"
#include "Misc/TextFilterUtils.h"
#include "Playlist/AvalanchePage.h"
#include "Playlist/Factories/Filters/AvaPlaylistFactoriesUtils.h"

const FName FAvaPlaylistFilterChannelExpressionFactory::KeyName = FName(TEXT("CHANNEL"));

FName FAvaPlaylistFilterChannelExpressionFactory::GetFilterIdentifier() const
{
	return KeyName;
}

bool FAvaPlaylistFilterChannelExpressionFactory::FilterExpression(const FAvalanchePage& InItem, const FAvaPlaylistTextFilterArgs& InArgs) const
{
	return InArgs.ValueToCheck.CompareName(InItem.GetChannelName(), InArgs.ComparisonMode);
}

bool FAvaPlaylistFilterChannelExpressionFactory::SupportsComparisonOperation(ETextFilterComparisonOperation InComparisonOperation, EAvaPlaylistSearchListType InPlaylistSearchListType) const
{
	const bool bIsEqualityOperation = InComparisonOperation == ETextFilterComparisonOperation::Equal 
		|| InComparisonOperation == ETextFilterComparisonOperation::NotEqual;

	return bIsEqualityOperation 
	   && InPlaylistSearchListType == EAvaPlaylistSearchListType::Instanced;
}
