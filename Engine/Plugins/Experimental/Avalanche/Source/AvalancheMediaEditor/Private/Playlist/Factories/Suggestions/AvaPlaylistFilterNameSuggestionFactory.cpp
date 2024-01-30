// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaPlaylistFilterNameSuggestionFactory.h"
#include "Playlist/AvalanchePlaylist.h"
#include "Playlist/Factories/Filters/AvaPlaylistFactoriesUtils.h"
#include "SAssetSearchBox.h"

#define LOCTEXT_NAMESPACE "AvaPlaylistFilterNameSuggestionFactory"

const FName FAvaPlaylistFilterNameSuggestionFactory::KeyName = FName(TEXT("NAME"));

void FAvaPlaylistFilterNameSuggestionFactory::AddSuggestion(const TSharedRef<FAvaPlaylistFilterSuggestionPayload>& InPayload)
{
	const FAvalanchePage& PageItem = UAvalanchePlaylist::GetPageSafe(InPayload->Playlist, InPayload->ItemPageId);
	if (PageItem.IsValidPage())
	{
		const FText NameCategoryLabel = LOCTEXT("NameCategoryLabel", "Ava-Playlist-Name");
		const FText PageText = PageItem.GetPageDescription();

		FString PageName = TEXT("\"");
		PageName +=	PageItem.GetPageDescription().ToString();
		PageName.Append(TEXT("\""));

		FString NameSuggestion = FString::Printf(TEXT("Name=%s"), *PageName);
		const bool bIsFilterValueValid = InPayload->FilterValue.IsEmpty() || NameSuggestion.Contains(InPayload->FilterValue);

		if (bIsFilterValueValid && !InPayload->FilterCache.Contains(NameSuggestion))
		{
			InPayload->FilterCache.Add(NameSuggestion);
			InPayload->PossibleSuggestions.Add(FAssetSearchBoxSuggestion{MoveTemp(NameSuggestion), PageText, NameCategoryLabel});
		}
	}
}

bool FAvaPlaylistFilterNameSuggestionFactory::SupportSuggestionType(EAvaPlaylistSearchListType InSuggestionType) const
{
	return InSuggestionType != EAvaPlaylistSearchListType::None;
}

#undef LOCTEXT_NAMESPACE
