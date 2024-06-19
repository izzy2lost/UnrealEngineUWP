// Copyright Epic Games, Inc. All Rights Reserved.

#include "ColorGradingListItem.h"

TArray<FColorGradingListItemGeneratorRef> FColorGradingListItem::RegisteredListItemGenerators;
TSet<TSubclassOf<AActor>> FColorGradingListItem::ActorClassesWithListItemGenerators;

TArray<FColorGradingListItemRef> FColorGradingListItem::GenerateColorGradingListItems(AActor* InActor)
{
	TArray<FColorGradingListItemRef> ListItems;

	for (const FColorGradingListItemGeneratorRef& GeneratorInstance : RegisteredListItemGenerators)
	{
		if (!GeneratorInstance.IsValid())
		{
			continue;
		}

		GeneratorInstance->GenerateColorGradingListItems(InActor, ListItems);
	}

	return ListItems;
}