// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaOutlinerDefines.h"
#include "Containers/ContainersFwd.h"

namespace UE::AvalancheOutliner
{
	/** Returns two subset arrays of Items: one is containing only Sortable Items and the other Non Sortable Items */
	AVALANCHEOUTLINER_API void SplitItems(const TArray<FAvaOutlinerItemPtr>& InItems
		, TArray<FAvaOutlinerItemPtr>& OutSortable
		, TArray<FAvaOutlinerItemPtr>& OutUnsortable);
}
