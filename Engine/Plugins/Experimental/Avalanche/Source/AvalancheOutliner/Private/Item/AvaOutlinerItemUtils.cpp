// Copyright Epic Games, Inc. All Rights Reserved.

#include "Item/AvaOutlinerItemUtils.h"
#include "Containers/Array.h"
#include "Item/IAvaOutlinerItem.h"

void UE::AvalancheOutliner::SplitItems(const TArray<FAvaOutlinerItemPtr>& InItems
	, TArray<FAvaOutlinerItemPtr>& OutSortable
	, TArray<FAvaOutlinerItemPtr>& OutUnsortable)
{
	// Allocate both for worst case scenarios
	OutSortable.Reserve(InItems.Num());
	OutUnsortable.Reserve(InItems.Num());

	for (const FAvaOutlinerItemPtr& Item : InItems)
	{
		if (!Item.IsValid())
		{
			continue;
		}

		if (Item->IsSortable())
		{
			OutSortable.Add(Item);
		}
		else
		{
			OutUnsortable.Add(Item);
		}
	}
}
