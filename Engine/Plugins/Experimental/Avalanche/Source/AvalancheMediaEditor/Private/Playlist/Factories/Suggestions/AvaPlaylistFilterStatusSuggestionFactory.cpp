// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaPlaylistFilterStatusSuggestionFactory.h"
#include "Playlist/AvalanchePlaylist.h"
#include "Playlist/Factories/Filters/AvaPlaylistFactoriesUtils.h"
#include "SAssetSearchBox.h"

#define LOCTEXT_NAMESPACE "AvaPlaylistFilterStatusSuggestionFactory"

const FName FAvaPlaylistFilterStatusSuggestionFactory::KeyName = FName(TEXT("STATUS"));

void FAvaPlaylistFilterStatusSuggestionFactory::AddSuggestion(const TSharedRef<FAvaPlaylistFilterSuggestionPayload>& InPayload)
{
	const FAvalanchePage& PageItem = UAvalanchePlaylist::GetPageSafe(InPayload->Playlist, InPayload->ItemPageId);
	if (PageItem.IsValidPage())
	{
		const FText StatusCategoryLabel = LOCTEXT("StatusCategoryLabel", "Ava-Rundown-Status");
		TArray<FAvalanchePageStatus> StatusPages = PageItem.GetPageContextualStatuses(InPayload->Playlist);
		for (FAvalanchePageStatus Status : StatusPages)
		{
			const FString StatusName = StaticEnum<EAvalanchePageStatus>()->GetNameStringByValue(static_cast<int32>(Status.Status));

			FString StatusNameSuggestion = FString::Printf(TEXT("Status=%s"), *StatusName);
			const bool bIsFilterValueValid = InPayload->FilterValue.IsEmpty() || StatusNameSuggestion.Contains(InPayload->FilterValue);

			if (bIsFilterValueValid && !InPayload->FilterCache.Contains(StatusNameSuggestion))
			{
				InPayload->FilterCache.Add(StatusNameSuggestion);
				InPayload->PossibleSuggestions.Add(FAssetSearchBoxSuggestion{MoveTemp(StatusNameSuggestion), FText::FromString(StatusName), StatusCategoryLabel});
			}
		}
	}
}

bool FAvaPlaylistFilterStatusSuggestionFactory::SupportSuggestionType(EAvaPlaylistSearchListType InSuggestionType) const
{
	return InSuggestionType != EAvaPlaylistSearchListType::None;
}

#undef LOCTEXT_NAMESPACE
