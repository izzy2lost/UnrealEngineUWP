// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaPlaylistFilterStatusExpressionFactory.h"
#include "Misc/TextFilterUtils.h"
#include "Playlist/AvalanchePage.h"

const FName FAvaPlaylistFilterStatusExpressionFactory::KeyName = FName(TEXT("STATUS"));

FName FAvaPlaylistFilterStatusExpressionFactory::GetFilterIdentifier() const
{
	return KeyName;
}

bool FAvaPlaylistFilterStatusExpressionFactory::FilterExpression(const FAvalanchePage& InItem, const FAvaPlaylistTextFilterArgs& InArgs) const
{
	if (!InArgs.ItemPlaylist)
	{
		return false;
	}
	const TArray<FAvalanchePageStatus> Statuses = InItem.GetPageContextualStatuses(InArgs.ItemPlaylist);
	for (const FAvalanchePageStatus& Status : Statuses)
	{
		const FName StatusName = StaticEnum<EAvalanchePageStatus>()->GetNameByValue(static_cast<int32>(Status.Status));
		if (InArgs.ValueToCheck.CompareName(StatusName, InArgs.ComparisonMode))
		{
			return true;
		}
	}
	return false;
}

bool FAvaPlaylistFilterStatusExpressionFactory::SupportsComparisonOperation(ETextFilterComparisonOperation InComparisonOperation, EAvaPlaylistSearchListType InPlaylistSearchListType) const
{
	return InComparisonOperation == ETextFilterComparisonOperation::Equal || InComparisonOperation == ETextFilterComparisonOperation::NotEqual;
}
